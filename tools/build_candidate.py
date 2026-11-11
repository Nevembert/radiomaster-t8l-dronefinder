#!/usr/bin/env python3
"""Build an OFFLINE experimental image. No device access or flashing.

Only the hash-locked official v1.7 image is accepted. Version 1.0 packages the audio build; full hardware behavior and independent
recovery remain unvalidated.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess

from analyze_v17 import analyze, BASE

ROOT = Path(__file__).resolve().parents[1]
MODULE = 0x0801D150
SETTINGS = 0x0801F000
HEAP_LITERAL = 0x0801BA8C
HEAP_OLD = 0x20003038
HEAP_NEW = 0x20002FB8
LED_CALLS = (0x08011FE0,0x0801387C,0x0801388E,0x080138C4,0x080138D6,
             0x08013B44,0x08013B56,0x08013B6A,0x08013C42,0x08013CCE,
             0x08013D74,0x08013E3E,0x08013F9C,0x080168A0,0x080168CA,
             0x080168E8,0x0801690C)
CALLS = ((0x0801743C,0x0800F6FC,"hook_init"),
         (0x0801725E,0x08014AA8,"hook_loop"),
         (0x08014BA4,0x08011D20,"hook_keys"),
         (0x080151B8,0x08017FC0,"hook_crsf")) + tuple(
             (address,0x0800F994,"hook_leds") for address in LED_CALLS)


def encode_bl(address, target):
    if address & 1 or target & 1:
        raise ValueError("Thumb BL instruction addresses must be even")
    displacement=target-address-4
    if not -(1<<24)<=displacement<(1<<24):
        raise ValueError("Thumb BL target out of range")
    value=displacement & 0x1ffffff
    sign=(value>>24)&1
    j1=1^((value>>23)&1)^sign
    j2=1^((value>>22)&1)^sign
    first=0xf000|(sign<<10)|((value>>12)&0x3ff)
    second=0xd000|(j1<<13)|(j2<<11)|((value>>1)&0x7ff)
    return struct.pack("<HH",first,second)


def decode_bl(address, instruction):
    first,second=struct.unpack("<HH",instruction)
    if first&0xf800!=0xf000 or second&0xd000!=0xd000:
        raise ValueError("not a Thumb BL")
    sign=(first>>10)&1
    i1=1^((second>>13)&1)^sign
    i2=1^((second>>11)&1)^sign
    value=(sign<<24)|(i1<<23)|(i2<<22)|((first&0x3ff)<<12)|((second&0x7ff)<<1)
    if sign: value-=1<<25
    return address+4+value


def patch_image(original, module, symbols):
    analyze(original)  # Mandatory BEFORE any address-map writes.
    if not module or MODULE+len(module)>SETTINGS:
        raise ValueError("module empty or overlaps settings")
    image=bytearray(original)
    patches=[]
    for address, old_target, name in CALLS:
        if name not in symbols:
            raise ValueError("missing entry point: "+name)
        target=symbols[name]&~1
        if not MODULE<=target<MODULE+len(module):
            raise ValueError("entry point outside module")
        offset=address-BASE
        old=image[offset:offset+4]
        if old!=encode_bl(address,old_target):
            raise ValueError("original call-site bytes differ")
        replacement=encode_bl(address,target)
        if decode_bl(address,replacement)!=target:
            raise ValueError("branch encoding mismatch")
        image[offset:offset+4]=replacement
        patches.append({"address":hex(address),"old":old.hex(),"new":replacement.hex(),
                        "function":name,"original_target":hex(old_target),"target":hex(target)})
    offset=HEAP_LITERAL-BASE
    old=image[offset:offset+4]
    if old!=struct.pack("<I",HEAP_OLD): raise ValueError("heap-end literal differs")
    new=struct.pack("<I",HEAP_NEW)
    image[offset:offset+4]=new
    patches.append({"address":hex(HEAP_LITERAL),"old":old.hex(),"new":new.hex(),
                    "purpose":"reserve 128 bytes inside original zero-initialized heap tail"})
    image.extend(b"\xff"*(MODULE-BASE-len(image)))
    image.extend(module)
    if image[:0x148]!=original[:0x148]: raise ValueError("interrupt vectors changed")
    if BASE+len(image)>SETTINGS: raise ValueError("settings boundary crossed")
    return bytes(image),patches


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware",required=True,type=Path)
    parser.add_argument("--toolchain-prefix",default="arm-none-eabi-")
    parser.add_argument("--output",type=Path,default=ROOT/"build")
    args=parser.parse_args()
    original=args.firmware.read_bytes()
    analyze(original)
    output=args.output.resolve(); output.mkdir(parents=True,exist_ok=True)
    compiler=args.toolchain_prefix+"gcc"
    common=[compiler,"-std=c11","-Os","-g0","-mcpu=cortex-m4","-mthumb",
            "-mfloat-abi=soft","-ffreestanding","-fno-builtin","-fno-common",
            "-ffunction-sections","-fdata-sections","-fno-unwind-tables",
            "-fno-asynchronous-unwind-tables","-Wall","-Wextra","-Werror",
            "-I"+str(ROOT/"src"),"-I"+str(ROOT/"firmware")]
    if Path(compiler).is_absolute(): common.insert(1,"-B"+str(Path(compiler).parent)+"/")
    command=common+["-nostdlib","-Wl,--gc-sections,-Map="+str(output/"module.map"),
                    "-T"+str(ROOT/"firmware/module.ld")]
    command += [str(p) for p in sorted((ROOT/"src").glob("*.c"))]
    command += [str(ROOT/"firmware/hooks.c"),str(ROOT/"firmware/freestanding.c"),
                "-o",str(output/"module.elf")]
    subprocess.run(command,check=True,timeout=60)
    subprocess.run([args.toolchain_prefix+"objcopy","-O","binary","--only-section=.text",
                    str(output/"module.elf"),str(output/"module.bin")],check=True,timeout=10)
    nm=subprocess.run([args.toolchain_prefix+"nm","-n",str(output/"module.elf")],
                      check=True,capture_output=True,text=True,timeout=10).stdout
    symbols={}
    for line in nm.splitlines():
        parts=line.split()
        if len(parts)==3: symbols[parts[2]]=int(parts[0],16)
    if symbols.get("runtime")!=HEAP_NEW: raise ValueError("unexpected runtime placement")
    module=(output/"module.bin").read_bytes()
    candidate,patches=patch_image(original,module,symbols)
    name="T8L_v17_DroneFinder_1.0.bin"
    (output/name).write_bytes(candidate)
    version=subprocess.run([compiler,"--version"],check=True,capture_output=True,text=True).stdout.splitlines()[0]
    report={"status":"VERSION 1.0; SOFTWARE CHECKED; FULL HARDWARE VALIDATION PENDING",
            "compiler":version,"original_sha256":hashlib.sha256(original).hexdigest(),
            "module_sha256":hashlib.sha256(module).hexdigest(),"module_bytes":len(module),
            "candidate_file":name,"candidate_bytes":len(candidate),
            "candidate_sha256":hashlib.sha256(candidate).hexdigest(),
            "module_start":hex(MODULE),"module_end":hex(MODULE+len(module)),
            "settings_start":hex(SETTINGS),"ram_reservation":[hex(HEAP_NEW),hex(HEAP_OLD)],
            "ymodem_padded_end":hex(BASE+((len(candidate)+1023)//1024)*1024),
            "patches":patches,"unvalidated":["hardware pin/order confirmation",
            "bootloader accepts modified image and recovery without opening case",
            "remaining heap headroom in all modes","ARM execution with original code/IRQs",
            "ELRS CH5 disarmed convention matches user's FC setup"]}
    (output/"candidate-report.json").write_text(json.dumps(report,indent=2)+"\n")
    print(json.dumps({k:report[k] for k in ("status","module_bytes","candidate_bytes","candidate_sha256")},indent=2))


if __name__=="__main__": main()
