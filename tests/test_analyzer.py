import importlib.util
from pathlib import Path
import sys
import unittest

root = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("analyzer", root/"tools/analyze_v17.py")
analyzer = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analyzer)
image_path = Path(sys.argv.pop(1)) if len(sys.argv)>1 else None


class AnalyzerTests(unittest.TestCase):
    def test_exact_image(self):
        result = analyzer.analyze(image_path.read_bytes())
        self.assertEqual(result["led_pins_in_logical_order"],
                         ["PB5","PB4","PB3","PA15","PD15","PD14"])
        self.assertEqual(result["compressed_data_end"], "0x801d142")
        self.assertEqual(result["timebase"]["nominal_ticks_per_second"],4000)
        self.assertEqual(result["keys"]["m"]["pin"],"PA5")
        self.assertEqual(result["keys"]["m"]["raw_address"],"0x20001b01")
        self.assertEqual(result["keys"]["power"]["pin"],"PC15")
        self.assertEqual(result["settings_flash_write_base"],"0x0801f800")

    def test_wrong_image_refused(self):
        image = image_path.read_bytes()
        for invalid in (b"", image[:-1], image+b"\x00", bytes([image[0]^1])+image[1:]):
            with self.assertRaises(ValueError):
                analyzer.analyze(invalid)


if __name__ == "__main__":
    if image_path is None:
        raise SystemExit("usage: test_analyzer.py OFFICIAL_V17_IMAGE")
    unittest.main()
