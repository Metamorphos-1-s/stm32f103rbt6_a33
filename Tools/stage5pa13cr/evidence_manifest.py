#!/usr/bin/env python3
"""Hash exact A13C-R files; optionally verify manifest without device access."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DIRECTORY = ROOT / 'Results/stage5pa13c_r'
MANIFEST = DIRECTORY / 'manifest.json'


def digest(path):
    sha = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b''):
            sha.update(block)
    return sha.hexdigest().upper()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    if args.verify:
        data = json.loads(MANIFEST.read_text(encoding='utf-8'))
        failed = []
        for item in data['files']:
            path = ROOT / item['path']
            if not path.exists() or path.stat().st_size != item['bytes'] or digest(path) != item['sha256']:
                failed.append(item['path'])
        print(json.dumps(dict(files=len(data['files']), failures=failed,
                             status='FAIL' if failed else 'PASS')))
        if failed:
            raise SystemExit(1)
    else:
        files = [p for p in DIRECTORY.rglob('*') if p.is_file() and p != MANIFEST]
        files += list((ROOT / 'Tools/stage5pa13cr').glob('*.py'))
        files += [ROOT / 'Docs/STAGE5PA13CR_FOCUSED_HARDWARE_RETEST.md',
                  ROOT / 'Results/stage5pa13c/software/firmware_0x051D_off_bootfix_UNFLASHED.bin',
                  ROOT / 'Results/stage5pa13c/software/firmware_0x051D_a13c_shadow.bin']
        entries = [dict(path=str(p.relative_to(ROOT)).replace('\\', '/'),
                        bytes=p.stat().st_size, sha256=digest(p)) for p in sorted(files)]
        MANIFEST.write_text(json.dumps(dict(classification='EXACT_BYTE_EVIDENCE',
            starting_head='5527d99046375531722443e576c162595dca2e6b', files=entries),
            indent=2) + '\n', encoding='utf-8')
        print(json.dumps(dict(files=len(entries), manifest=str(MANIFEST))))


if __name__ == '__main__':
    main()
