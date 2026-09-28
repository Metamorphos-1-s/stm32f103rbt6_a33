"""Hash and verify software-only negative qualification evidence; never open COM5."""
import argparse
import hashlib
import json
from pathlib import Path

ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'Results/stage5pa13e'
MANIFEST=BASE/'manifest.json'

def files():
    return sorted(path for path in BASE.rglob('*') if path.is_file() and path!=MANIFEST)

def create():
    entries=[]
    for path in files():
        data=path.read_bytes()
        entries.append(dict(path=path.relative_to(ROOT).as_posix(),bytes=len(data),
            sha256=hashlib.sha256(data).hexdigest().upper()))
    MANIFEST.write_text(json.dumps(dict(schema=1,classification='FAILED_SOFTWARE_GATE_NO_DEVICE_ACCESS',
        baseline='7ccfe181d81079e8f1b670a7fedb574d0b79cbfe',entries=entries),indent=2)+'\n')

def verify():
    manifest=json.loads(MANIFEST.read_text())
    if manifest['classification']!='FAILED_SOFTWARE_GATE_NO_DEVICE_ACCESS':raise ValueError('classification changed')
    for entry in manifest['entries']:
        path=(ROOT/entry['path']).resolve()
        if ROOT.resolve() not in path.parents:raise ValueError('outside repository')
        data=path.read_bytes()
        if len(data)!=entry['bytes'] or hashlib.sha256(data).hexdigest().upper()!=entry['sha256']:
            raise ValueError('SHA mismatch '+entry['path'])
    return len(manifest['entries'])

if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--create',action='store_true');args=parser.parse_args()
    if args.create:create()
    print('verified FAIL evidence artifacts:',verify())
