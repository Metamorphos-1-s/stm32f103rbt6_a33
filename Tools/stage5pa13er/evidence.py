"""Portable SHA manifest: preserve initial FAIL and aborted target capture."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'Results/stage5pa13er'
MANIFEST=BASE/'manifest.json'

def selected():
    paths={p for p in BASE.rglob('*') if p.is_file() and p!=MANIFEST}
    paths.update((ROOT/'Tools/stage5pa13er').glob('*.py'))
    paths.update((ROOT/'Docs').glob('STAGE5PA13ER*.md'))
    return sorted(paths)

def create():
    entries=[]
    for path in selected():
        data=path.read_bytes()
        if len(data)>=95_000_000:raise ValueError('file exceeds Git limit: '+str(path))
        entries.append(dict(path=path.relative_to(ROOT).as_posix(),bytes=len(data),
            sha256=hashlib.sha256(data).hexdigest().upper()))
    MANIFEST.write_text(json.dumps(dict(schema=1,classification='ABORTED_RESOURCE_DIAGNOSTIC_NOT_0520_READY',
        baseline='72a2283ec3073d61834a5c2d13a9a5d1ad1c19da',
        plan_and_software_commit='fbec3aed80ce8cf9db452e1ba6914bf7c9a8d848',
        items=entries),indent=2)+'\n')

def verify():
    entries=json.loads(MANIFEST.read_text())['items']
    for entry in entries:
        path=(ROOT/entry['path']).resolve()
        if ROOT.resolve() not in path.parents:raise ValueError('outside repository')
        data=path.read_bytes()
        if len(data)!=entry['bytes'] or hashlib.sha256(data).hexdigest().upper()!=entry['sha256']:
            raise ValueError('artifact mismatch '+entry['path'])
    return len(entries)

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--create',action='store_true')
    args=parser.parse_args()
    if args.create:create()
    print('PASS: verified',verify(),'failed and successful evidence files')
