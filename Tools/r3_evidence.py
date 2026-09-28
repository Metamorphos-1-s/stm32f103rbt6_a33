"""Portable R3 manifest; preserves failure files, excludes .pyc and user work."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'Results/stage5pa13er3'
MANIFEST=BASE/'manifest.json'

def selected():
    paths={p for p in BASE.rglob('*') if p.is_file() and p!=MANIFEST and p.suffix!='.pyc'}
    paths.update((ROOT/'Docs').glob('STAGE5PA13ER3*.md'))
    paths.update((ROOT/'Tools').glob('r3_*.py'))
    paths.update((ROOT/'Tools').glob('test_r3_*.py'))
    paths.add(ROOT/'Tools/h1_atomic.py')
    return sorted(paths)

def create():
    entries=[]
    for path in selected():
        data=path.read_bytes()
        if len(data)>=95000000:raise ValueError('artifact too large')
        entries.append(dict(path=path.relative_to(ROOT).as_posix(),bytes=len(data),
            sha256=hashlib.sha256(data).hexdigest().upper()))
    MANIFEST.write_text(json.dumps(dict(schema=1,baseline='befb8835144a1d3b4b101d7c12f960c5f81c10be',
        stage='A13E-R3',classification='INCOMPLETE_H1_NONZERO_RAW_GATE; CLEAN_TERMINAL',
        entries=entries),indent=2)+'\n',encoding='utf-8')

def verify():
    entries=json.loads(MANIFEST.read_text())['entries']
    for entry in entries:
        path=(ROOT/entry['path']).resolve()
        if ROOT.resolve() not in path.parents:raise ValueError('outside repository')
        data=path.read_bytes()
        if len(data)!=entry['bytes'] or hashlib.sha256(data).hexdigest().upper()!=entry['sha256']:
            raise ValueError('artifact mismatch: '+entry['path'])
    return len(entries)

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--create',action='store_true');a=p.parse_args()
    if a.create:create()
    print('Verified',verify(),'R3 artifacts (including original failures/empty truncated evidence)')
