#!/usr/bin/env python3
"""Build and run the project's own native logic tests; never executes firmware."""
import argparse
import os
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
CASES = {
    "audio": ["finder_audio"],
    "finder": ["finder"],
    "adapter": ["finder", "t8l_adapter"],
    "m_button": ["m_button"],
    "clock": ["t8l_clock"],
    "m_bridge": ["m_button", "t8l_clock", "t8l_m_bridge"],
    "hooks": ["finder", "finder_audio", "t8l_adapter", "m_button", "t8l_clock", "t8l_m_bridge"],
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firmware", type=Path, help="optional official v1.7, read only")
    args = parser.parse_args()
    environment = dict(os.environ, ASAN_OPTIONS="detect_leaks=0")
    with tempfile.TemporaryDirectory(prefix="t8l-own-tests-") as directory:
        for name, sources in CASES.items():
            output = Path(directory) / name
            command = ["cc", "-std=c11", "-Wall", "-Wextra", "-Werror",
                       "-fsanitize=address,undefined", "-I", str(ROOT / "src")]
            command += [str(ROOT / "src" / (source + ".c")) for source in sources]
            if name == "hooks":
                command += ["-DT8L_HOST_TEST", "-I", str(ROOT/"firmware"),
                            str(ROOT/"firmware/hooks.c")]
            command += [str(ROOT / "tests" / ("test_" + name + ".c")), "-o", str(output)]
            subprocess.run(command, check=True, timeout=60)
            subprocess.run([str(output)], check=True, timeout=10, env=environment)
    if args.firmware:
        subprocess.run([sys.executable, str(ROOT / "tests/test_analyzer.py"),
                        str(args.firmware.resolve())], check=True, timeout=10)
    print("PASS: seven native suites (ASan/UBSan)" +
          (" and two static firmware checks" if args.firmware else ""))


if __name__ == "__main__":
    main()
