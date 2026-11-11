"""Static verification of patched bytes and own ARM code, not ARM execution.
Uses GNU objdump for an independent Thumb decoder.
"""
import hashlib
import json
from pathlib import Path
import struct
import re
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/"tools"))
import build_candidate as builder
from analyze_v17 import analyze, decode_data, BASE

ORIGINAL=Path(sys.argv.pop(1)).read_bytes()
BUILD=Path(sys.argv.pop(1))
OBJDUMP=sys.argv.pop(1) if len(sys.argv)>1 else "arm-none-eabi-objdump"
REPORT=json.loads((BUILD/"candidate-report.json").read_text())
CANDIDATE=(BUILD/REPORT["candidate_file"]).read_bytes()

def independent_target(instruction,address):
    with tempfile.TemporaryDirectory(prefix="t8l-decode-") as directory:
        path=Path(directory)/"instruction.bin"; path.write_bytes(instruction)
        result=subprocess.run([OBJDUMP,"-D","-b","binary","-m","arm","-M","force-thumb",
                               "--adjust-vma="+str(address),str(path)],
                              check=True,capture_output=True,text=True,timeout=5)
    matches=re.findall(r"\tbl\s+(?:0x)?([0-9a-f]+)",result.stdout)
    if len(matches)!=1: raise ValueError("expected one independently decoded BL")
    return int(matches[0],16)


class CandidateTests(unittest.TestCase):
    def test_stock_audio_calls_unmodified(self):
        # Single beep: duration=50, start callback=08010115, then duration=1
        # stop callback=0801013d. Both invoke the native queue wrapper.
        self.assertEqual(ORIGINAL[0x080101de-BASE:0x080101e0-BASE],bytes.fromhex('32 20'))
        for address in (0x080101e0,0x080101f2):
            self.assertEqual(independent_target(ORIGINAL[address-BASE:address-BASE+4],address),0x0801a638)
        self.assertEqual(CANDIDATE[0x080100d8-BASE:0x08010548-BASE],
                         ORIGINAL[0x080100d8-BASE:0x08010548-BASE])
        self.assertEqual(CANDIDATE[0x08015ab4-BASE:0x08015d20-BASE],
                         ORIGINAL[0x08015ab4-BASE:0x08015d20-BASE])

    def test_hashes_vectors_and_size(self):
        analyze(ORIGINAL)
        self.assertEqual(hashlib.sha256(CANDIDATE).hexdigest(),REPORT["candidate_sha256"])
        self.assertEqual(CANDIDATE[:0x148],ORIGINAL[:0x148])
        self.assertLessEqual(BASE+len(CANDIDATE),builder.SETTINGS)
        self.assertLessEqual(BASE+((len(CANDIDATE)+1023)//1024)*1024,builder.SETTINGS)

    def test_every_modified_byte_accounted_for(self):
        allowed=set()
        for p in REPORT["patches"]:
            offset=int(p["address"],16)-BASE
            self.assertEqual(ORIGINAL[offset:offset+4].hex(),p["old"])
            self.assertEqual(CANDIDATE[offset:offset+4].hex(),p["new"])
            allowed.update(range(offset,offset+4))
        actual={i for i,(a,b) in enumerate(zip(ORIGINAL,CANDIDATE)) if a!=b}
        self.assertTrue(actual<=allowed)
        self.assertEqual(len(REPORT["patches"]),22)
        self.assertEqual(CANDIDATE[len(ORIGINAL):builder.MODULE-BASE],b"\xff"*8)
        self.assertEqual(CANDIDATE[builder.MODULE-BASE:],(BUILD/"module.bin").read_bytes())

    def test_independent_branch_decoder(self):
        for p in REPORT["patches"]:
            if "target" not in p: continue
            address=int(p["address"],16)
            for field,target in (("old","original_target"),("new","target")):
                self.assertEqual(independent_target(bytes.fromhex(p[field]),address),int(p[target],16))

    def test_branch_extremes(self):
        address=0x10000000
        for delta in (-(1<<24),-0x800000,-4096,-2,0,2,4096,0x800000,(1<<24)-2):
            target=address+4+delta
            instruction=builder.encode_bl(address,target)
            self.assertEqual(independent_target(instruction,address),target)
        for target in (address+5,address+4+(1<<24),address+2-(1<<24)):
            with self.assertRaises(ValueError): builder.encode_bl(address,target)

    def test_heap_reservation(self):
        old=struct.unpack_from("<I",ORIGINAL,builder.HEAP_LITERAL-BASE)[0]
        new=struct.unpack_from("<I",CANDIDATE,builder.HEAP_LITERAL-BASE)[0]
        self.assertEqual(old-new,128)
        self.assertEqual(new,0x20002fb8)
        self.assertEqual(CANDIDATE[0x158:0x15c],ORIGINAL[0x158:0x15c]) # MSP literal

    def test_bad_inputs_refused(self):
        module=(BUILD/"module.bin").read_bytes()
        symbols={p["function"]:int(p["target"],16) for p in REPORT["patches"] if "function" in p}
        with self.assertRaises(ValueError): builder.patch_image(ORIGINAL+b"x",module,symbols)
        with self.assertRaises(ValueError): builder.patch_image(ORIGINAL,b"",symbols)
        with self.assertRaises(ValueError): builder.patch_image(ORIGINAL,module,{})
        bad=dict(symbols,hook_loop=builder.SETTINGS)
        with self.assertRaises(ValueError): builder.patch_image(ORIGINAL,module,bad)
        with self.assertRaises(ValueError):
            builder.patch_image(ORIGINAL,bytes(builder.SETTINGS-builder.MODULE+1),symbols)

    def test_actual_loop_hook(self):
        p=next(p for p in REPORT["patches"] if p.get("function")=="hook_loop")
        self.assertEqual(int(p["address"],16),0x0801725e)
        self.assertEqual(int(p["original_target"],16),0x08014aa8)
        # The outer entry never returns during normal operation.
        self.assertEqual(CANDIDATE[0x08017442-BASE:0x08017446-BASE],
                         ORIGINAL[0x08017442-BASE:0x08017446-BASE])

    def test_bench_query_dispatch(self):
        data,_=decode_data(ORIGINAL)
        self.assertEqual(data[0x3f6:0x3fb],bytes.fromhex('a5 55 26 0d 0a'))
        # Parser's successful match returns internal command 0x33 (MOVS r0,#51).
        self.assertEqual(ORIGINAL[0x0800ffe6-BASE:0x0800ffe8-BASE],bytes.fromhex('33 20'))
        table=0x0801af2c
        offset=struct.unpack_from('<H',ORIGINAL,table-BASE+0x33*2)[0]
        self.assertEqual(table+2*offset,0x0801b8c2)
        self.assertEqual(independent_target(ORIGINAL[0x0801b8c2-BASE:0x0801b8c6-BASE],
                                           0x0801b8c2),0x0801942c)


if __name__=="__main__": unittest.main()
