#!/usr/bin/env python3
"""Verify the published manifest and package version 1.0 without flashing."""
import hashlib
import json
from pathlib import Path, PurePosixPath
import shutil
import zipfile

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = 'build/T8L_v17_DroneFinder_1.0.bin'
EXPECTED = '7487229fd06aa87484c529ee938b3c2111ff8a9ca9b7af36e5073575a3ce88e6'


def main():
    if (ROOT / 'VERSION').read_text().strip() != '1.0.0':
        raise ValueError('This packager targets the reviewed version 1.0.0 only')
    manifest = json.loads((ROOT / 'SHA256SUMS.json').read_text())
    if manifest.get(FIRMWARE) != EXPECTED:
        raise ValueError('Release firmware hash differs')
    for name, expected in manifest.items():
        parts = PurePosixPath(name)
        if parts.is_absolute() or '..' in parts.parts:
            raise ValueError('Invalid manifest path')
        path = ROOT / name
        if path.is_symlink() or not path.resolve().is_relative_to(ROOT):
            raise ValueError('Manifest path escapes project')
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError('Hash mismatch: ' + name)
    dest = ROOT / 'dist'
    dest.mkdir(exist_ok=True)
    firmware = dest / Path(FIRMWARE).name
    shutil.copyfile(ROOT / FIRMWARE, firmware)
    archive = dest / 'T8L_DroneFinder_1.0.zip'
    with zipfile.ZipFile(archive, 'w', compression=zipfile.ZIP_DEFLATED) as out:
        for name in sorted([*manifest, 'SHA256SUMS.json']):
            entry = zipfile.ZipInfo('T8L_DroneFinder_1.0/' + name, (2026, 11, 11, 0, 0, 0))
            entry.compress_type = zipfile.ZIP_DEFLATED
            entry.external_attr = 0o100644 << 16
            out.writestr(entry, (ROOT / name).read_bytes())
    with zipfile.ZipFile(archive) as check:
        if check.testzip() is not None:
            raise ValueError('ZIP validation failed')
    (dest / 'SHA256SUMS.txt').write_text(''.join(
        hashlib.sha256(f.read_bytes()).hexdigest() + '  ' + f.name + '\n'
        for f in (firmware, archive)))
    print(f'Verified {len(manifest)} files; created {archive.name} and {firmware.name}')


if __name__ == '__main__':
    main()
