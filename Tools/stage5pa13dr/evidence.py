"""Select formal artifacts without archiving temporary builds or user files."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
BASE = ROOT / 'Results/stage5pa13dr'
MANIFEST = BASE / 'manifest.json'


def selected():
    paths = set()
    for path in (BASE / 'hardware').rglob('*'):
        if path.is_file():
            paths.add(path)
    for path in (BASE / 'software').rglob('*'):
        if not path.is_file():
            continue
        relative = path.relative_to(BASE / 'software')
        if len(relative.parts) == 1 or ('analysis' in relative.parts[0] and path.suffix == '.json'):
            paths.add(path)
        elif relative.parts[0] in ('arm_debug', 'arm_release', 'baseline_release'):
            if path.suffix in ('.map', '.elf', '.su', '.ci') or path.name in ('CMakeCache.txt', 'compile_commands.json') or path.name == 'a13c_shadow_compensator.c.obj':
                paths.add(path)
    paths.update(path for path in BASE.glob('*.json') if path != MANIFEST)
    paths.update((ROOT / 'Docs').glob('STAGE5PA13DR*.md'))
    paths.update((ROOT / 'Tools/stage5pa13dr').glob('*.py'))
    return sorted(paths)


def create():
    entries = []
    for path in selected():
        data = path.read_bytes()
        if len(data) > 95_000_000:
            raise ValueError('artifact too large: '+str(path))
        entries.append(dict(path=path.relative_to(ROOT).as_posix(), bytes=len(data),
                            sha256=hashlib.sha256(data).hexdigest().upper()))
    MANIFEST.write_text(json.dumps(dict(schema=1, baseline='5dd51a472aabd4a14a2b69701dbcc70baf687b4d',
        software_contract_commit='5ab919ff475bcbd58412581f49b89ad5289f11fa',
        entries=entries), indent=2)+'\n', encoding='utf-8')
    return len(entries)


def verify():
    entries = json.loads(MANIFEST.read_text())['entries']
    for entry in entries:
        path = (ROOT / entry['path']).resolve()
        if ROOT.resolve() not in path.parents:
            raise ValueError('outside repository')
        data = path.read_bytes()
        if len(data) != entry['bytes'] or hashlib.sha256(data).hexdigest().upper() != entry['sha256']:
            raise ValueError('manifest mismatch: '+entry['path'])
    return len(entries)


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--create', action='store_true')
    args = parser.parse_args()
    if args.create:
        create()
    print('PASS: verified', verify(), 'formal artifacts')
