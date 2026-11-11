"""Execute ONLY our compiled ARM module in Unicorn, with six mocked stock calls.

No original firmware bytes are mapped or executed. This is an ABI/instruction
test, not an MCU, interrupt scheduler, bootloader or RF simulation.
Requires unicorn==2.1.4. Run after build_candidate.py.
"""
import json
from pathlib import Path
import struct
import sys
import unittest
from unicorn import Uc, UC_ARCH_ARM, UC_MODE_THUMB, UC_MODE_MCLASS, UC_HOOK_CODE, UC_HOOK_MEM_WRITE, UC_PROT_READ, UC_PROT_EXEC
from unicorn.arm_const import (UC_ARM_REG_R0, UC_ARM_REG_R1, UC_ARM_REG_R4,
    UC_ARM_REG_R5, UC_ARM_REG_R6, UC_ARM_REG_R7, UC_ARM_REG_R8, UC_ARM_REG_R9,
    UC_ARM_REG_R10, UC_ARM_REG_R11, UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_PC,
    UC_ARM_REG_PRIMASK, UC_CPU_ARM_CORTEX_M4)

BUILD=Path(sys.argv.pop(1)) if len(sys.argv)>1 else Path(__file__).resolve().parents[1]/'build'
REPORT=json.loads((BUILD/'candidate-report.json').read_text())
MODULE=(BUILD/'module.bin').read_bytes()
START=int(REPORT['module_start'],16)
SYMBOLS={p['function']:int(p['target'],16) for p in REPORT['patches'] if 'function' in p}
STOCK={int(p['original_target'],16):p['function'] for p in REPORT['patches'] if 'function' in p}
STOCK[0x08010198]='stock_beep'
END=0x08005000
SP=0x20004400
EVENTS=0x2000066d
FRAME=0x20001a58
LEDS=0x20001008
NORMAL=bytes([0,1,0,1,1,0])
CALLEE=(UC_ARM_REG_R4,UC_ARM_REG_R5,UC_ARM_REG_R6,UC_ARM_REG_R7,
        UC_ARM_REG_R8,UC_ARM_REG_R9,UC_ARM_REG_R10,UC_ARM_REG_R11)
MAX_STACK=0

def crc(data):
    v=0
    for byte in data:
        v^=byte
        for _ in range(8): v=((v<<1)^(0xd5 if v&128 else 0))&255
    return v

class Board:
    def __init__(self):
        self.u=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS)
        self.u.ctl_set_cpu_model(UC_CPU_ARM_CORTEX_M4)
        self.u.mem_map(0x08000000,0x20000)
        self.u.mem_write(START,MODULE)
        self.u.mem_protect(0x08000000,0x20000,UC_PROT_READ|UC_PROT_EXEC)
        self.u.mem_map(0x20000000,0x5000)
        self.output=NORMAL
        self.parser_result=13
        self.seen_m=None
        self.returned=False
        self.minimum_sp=SP
        self.calls=[]
        self.beeps=[]
        self.u.hook_add(UC_HOOK_CODE,self.code)
        self.u.hook_add(UC_HOOK_MEM_WRITE,self.write_guard)
        for a,v,n in [(0x20000e1c,0xfffffc17,4),(0x20000e00,0x20001b40,4),
            (0x20001b47,10,1),(0x200002b0,3,2),(0x200002b4,1,1),
            (0x2000065e,1,1),(0x20000690,988,4),(0x2000032c,660,2),
            (0x2000032e,840,2),(0x20001af4,800,2),(0x20001b01,1,1)]: self.put(a,v,n)
        self.u.mem_write(LEDS,NORMAL)
        for a,v,n in [(0x20000e20,0x20001d00,4),(0x20001d0c,32,4),
            (0x20000182,1,1),(0x20000004,1,4),(0x20000000,0x20001e04,4),
            (0x20001e04,0x400,4)]: self.put(a,v,n)
        assert self.call('hook_init')==0x12345678
        self.step(False,0)

    def put(self,a,v,n=4): self.u.mem_write(a,int(v).to_bytes(n,'little'))
    def get(self,a,n=4): return int.from_bytes(self.u.mem_read(a,n),'little')
    def write_guard(self,u,access,address,size,value,unused):
        # The module may mutate only its reserved state, its stack, and the
        # temporary M byte. Hardware/global configuration writes are forbidden.
        permitted=(0x20002fb8<=address and address+size<=0x20003038 or
                   SP-1024<=address and address+size<=SP or
                   address==EVENTS+1 and size==1)
        assert permitted, f'unexpected ARM write {address:#x}+{size}'

    def code(self,u,address,size,unused):
        self.minimum_sp=min(self.minimum_sp,u.reg_read(UC_ARM_REG_SP))
        if address==END:
            self.returned=True; u.emu_stop(); return
        if address in STOCK:
            name=STOCK[address]; self.calls.append(name)
            r0=u.reg_read(UC_ARM_REG_R0)
            if name=='hook_leds': self.output=bytes(u.mem_read(r0,6)); value=0x1234
            elif name=='hook_keys': self.seen_m=self.get(r0+1,1); value=0xabc
            elif name=='hook_crsf': value=self.parser_result
            elif name=='hook_init': value=0x12345678
            elif name=='stock_beep':
                assert r0==1 and u.reg_read(UC_ARM_REG_R1)==50
                assert u.reg_read(UC_ARM_REG_PRIMASK)==1
                self.beeps.append(self.get(0x20000e1c)); value=0
            else: value=0x87654321
            assert u.reg_read(UC_ARM_REG_SP)%8==0, 'AAPCS stack alignment at stock call'
            u.reg_write(UC_ARM_REG_R0,value)
            u.reg_write(UC_ARM_REG_PC,u.reg_read(UC_ARM_REG_LR))
            return
        assert START<=address<START+len(MODULE), f'escaped own module: {address:#x}'

    def call(self,name,a=0,b=0):
        global MAX_STACK
        u=self.u
        for i,r in enumerate(CALLEE): u.reg_write(r,0x41410000+i)
        u.reg_write(UC_ARM_REG_SP,SP)
        u.reg_write(UC_ARM_REG_R0,a); u.reg_write(UC_ARM_REG_R1,b)
        u.reg_write(UC_ARM_REG_LR,END|1)
        mask=u.reg_read(UC_ARM_REG_PRIMASK)
        self.returned=False
        u.emu_start(SYMBOLS[name]|1,0,count=100000,timeout=1000000)
        assert self.returned, 'instruction/time budget exhausted'
        assert u.reg_read(UC_ARM_REG_SP)==SP
        assert u.reg_read(UC_ARM_REG_PRIMASK)==mask
        for i,r in enumerate(CALLEE): assert u.reg_read(r)==0x41410000+i, (name,r)
        MAX_STACK=max(MAX_STACK,SP-self.minimum_sp)
        return u.reg_read(UC_ARM_REG_R0)

    def step(self,down,ms,event=0):
        self.put(0x20000e1c,(self.get(0x20000e1c)+ms*4)&0xffffffff)
        self.put(0x20001b01,int(not down),1); self.put(EVENTS+1,event,1)
        assert self.call('hook_keys',EVENTS)==0xabc
        assert self.call('hook_loop')==0x87654321
    def tap(self):
        for down,ms,event in [(True,1,1),(True,45,1),(False,55,2),(False,45,0)]: self.step(down,ms,event)
    def enter(self): self.step(False,45); self.tap(); self.tap()
    def frame(self,rssi,size=14,bad=False):
        f=bytearray([0xea,12,0x14,80,80,100,0,0,2,1,rssi&255,0,0,0])
        f[-1]=crc(f[2:-1]) ^ int(bad)
        self.u.mem_write(FRAME,bytes(f)+bytes(114))
        assert self.call('hook_crsf',FRAME,size)==self.parser_result
        self.step(False,50)

class ArmTests(unittest.TestCase):
    def test_audio_cadence_and_no_link(self):
        b=Board(); b.enter(); self.assertEqual(len(b.beeps),0)
        b.frame(-20); self.assertEqual(len(b.beeps),1)
        b.step(False,199); self.assertEqual(len(b.beeps),1)
        b.step(False,1); self.assertEqual(len(b.beeps),2)
        b.step(False,1500); self.assertEqual(len(b.beeps),2)
        b.frame(-20); self.assertEqual(len(b.beeps),3)
        b.tap(); b.tap(); before=len(b.beeps)
        b.frame(-20); b.step(False,1000); self.assertEqual(len(b.beeps),before)

    def test_audio_stock_priority_and_memory(self):
        for a,v,n in [(0x20000e20,0,4),(0x20001d08,1,4),(0x20000182,0,1),
            (0x20000e24,5,2),(0x20000e28,0x08010115,4),
            (0x20000000,0,4),(0x20001e04,40,4),(0x20001e08,0x20001e04,4),
            (0x20000000,0xfffffff0,4),(0x20001e04,0xffffffff,4)]:
            b=Board(); b.enter(); b.put(a,v,n); b.frame(-20)
            self.assertEqual(len(b.beeps),0,(a,v))
        b=Board(); b.enter()
        b.put(0x20001e04,24); b.put(0x20001e08,0x20001f04); b.put(0x20001f04,24)
        b.frame(-20); self.assertEqual(len(b.beeps),1)

    def test_four_levels_and_double_exit(self):
        for rssi,n in [(-100,1),(-80,2),(-60,3),(-25,4)]:
            b=Board(); b.enter(); b.frame(rssi)
            self.assertEqual(b.output,bytes([int(i<n) for i in range(4)])+NORMAL[4:])
            b.tap(); b.tap(); self.assertEqual(b.output,NORMAL)

    def test_real_receive_buffer_sizes(self):
        for n in (14,15,28,64,128):
            b=Board(); b.enter(); b.frame(-25,n)
            self.assertEqual(b.output[:4],bytes([1]*4))
        for n in (0,1,12,13,129):
            b=Board(); b.enter(); b.frame(-25,n)
            self.assertNotEqual(b.output[:4],bytes([1]*4))

    def test_invalid_crc_and_original_parser_failure(self):
        for bad,result in ((True,13),(False,255)):
            b=Board(); b.enter(); b.parser_result=result; b.frame(-25,bad=bad)
            self.assertNotEqual(b.output[:4],bytes([1]*4))

    def test_context_gates(self):
        for a,v,n in [(0x200002b0,2,2),(0x20000690,2012,4),(0x2000065f,1,1),
                     (0x2000065e,0,1),(0x20001af4,600,2),(0x20000e00,0,4)]:
            b=Board(); b.enter(); b.frame(-25); b.put(a,v,n)
            b.call('hook_leds',LEDS); self.assertEqual(b.output,NORMAL)
            before=len(b.beeps); b.frame(-20); b.step(False,600)
            self.assertEqual(len(b.beeps),before)

    def test_timeout_and_stalled_loop(self):
        b=Board(); b.enter(); b.frame(-25); b.step(False,1600)
        self.assertNotEqual(b.output[:4],bytes([1]*4))
        b.frame(-25); b.put(0x20000e1c,(b.get(0x20000e1c)+1001)&0xffffffff)
        b.call('hook_leds',LEDS); self.assertEqual(b.output,NORMAL)

    def test_masked_irq_state_and_event_restore(self):
        b=Board(); b.u.reg_write(UC_ARM_REG_PRIMASK,1)
        b.enter(); b.frame(-25); b.step(False,50,255)
        self.assertEqual(b.seen_m,0); self.assertEqual(b.get(EVENTS+1,1),255)
        self.assertEqual(b.u.reg_read(UC_ARM_REG_PRIMASK),1)

if __name__=='__main__':
    suite=unittest.defaultTestLoader.loadTestsFromTestCase(ArmTests)
    result=unittest.TextTestRunner(verbosity=2).run(suite)
    print(f'Max observed own-module stack depth: {MAX_STACK} bytes; stock functions mocked')
    sys.exit(not result.wasSuccessful())
