"""Hash only the explicit A13D delivery set; never alter original records."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DATA = ROOT / 'Results/stage5pa13d'


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(1048576), b''):
            h.update(block)
    return h.hexdigest().upper()


def paths():
    selected = list((DATA/'hardware').rglob('*'))
    selected += list(DATA.glob('*.json'))
    software = DATA/'software'
    selected += list(software.glob('*.json')) + list(software.glob('*.log'))
    selected += list(software.glob('*.txt')) + list(software.glob('*.bin'))
    for folder in software.iterdir():
        if folder.is_dir() and folder.name.endswith(('analysis', '_gate')):
            selected += list(folder.glob('*.json'))
    for name in ('arm_debug', 'arm_release', 'baseline_release'):
        build = software/name
        selected += [build/'stm32f103rbt6_a33.map', build/'stm32f103rbt6_a33.elf',
                     build/'compile_commands.json', build/'CMakeCache.txt']
        selected += list(build.rglob('*.ci')) + list(build.rglob('*.su'))
    selected += list((ROOT/'Tools/stage5pa13d').glob('*.py'))
    selected += list((ROOT/'Diagnostics').glob('stage5pa13d*'))
    selected += [ROOT/'Tests/host/stage5pa13d/test_resources.c']
    selected += list((ROOT/'Docs').glob('STAGE5PA13D*.md'))
    return sorted(set(p for p in selected if p.is_file() and p.name != 'manifest.json'))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--verify', action='store_true')
    args = parser.parse_args()
    manifest = DATA/'manifest.json'
    if args.verify:
        records = json.loads(manifest.read_text())['files']
        bad = []
        for r in records:
            p = ROOT/r['path']
            if not p.exists() or p.stat().st_size != r['bytes'] or digest(p) != r['sha256']:
                bad.append(r['path'])
        print(json.dumps(dict(files=len(records), failures=bad, result='FAIL' if bad else 'PASS')))
        if bad:
            raise SystemExit(1)
    else:
        records = [dict(path=str(p.relative_to(ROOT)).replace('\\','/'),
                        bytes=p.stat().st_size, sha256=digest(p)) for p in paths()]
        manifest.write_text(json.dumps(dict(classification='EXACT_BYTE_FAILURE_AND_RESTORE_EVIDENCE',
            baseline='4a6f3edfbab74a7fd7ba1ff35c768534c05d2704',
            result='A13D TARGET RESOURCE GATE FAIL', files=records), indent=2)+'\n')
        print(json.dumps(dict(files=len(records), manifest=str(manifest))))


if __name__ == '__main__':
    main()
