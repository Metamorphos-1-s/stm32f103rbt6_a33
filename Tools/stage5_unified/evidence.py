"""Create/verify a staged artifact manifest without touching the device."""
import argparse, hashlib, json, subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
BASE=ROOT/'Results/stage5p_unified'
MANIFEST=BASE/'manifest.json'
def entries():
    paths=[p for p in BASE.rglob('*') if p.is_file() and p.name!='manifest.json']
    paths += list((ROOT/'Docs').glob('STAGE5P_UNIFIED*.md'))
    paths += list((ROOT/'Tools/stage5_unified').glob('*.py'))
    return sorted(set(paths))
def git_bytes(path):
    rel=path.relative_to(ROOT).as_posix()
    try:return subprocess.check_output(['git','show',':'+rel],cwd=ROOT)
    except subprocess.CalledProcessError:return path.read_bytes()
def create():
    out=[]
    for path in entries():
        data=git_bytes(path)
        out.append(dict(path=path.relative_to(ROOT).as_posix(),bytes=len(data),sha256=hashlib.sha256(data).hexdigest().upper()))
    MANIFEST.write_text(json.dumps(dict(schema=1,stage='STAGE5P_UNIFIED',classification='SOFTWARE_PASS_HARDWARE_INCOMPLETE_UNFLASHED',entries=out),indent=2)+'\n',encoding='utf-8')
def verify():
    data=json.loads(MANIFEST.read_text(encoding='utf-8'))
    for entry in data['entries']:
        path=ROOT/entry['path'];blob=git_bytes(path)
        if len(blob)!=entry['bytes'] or hashlib.sha256(blob).hexdigest().upper()!=entry['sha256']:raise ValueError(entry['path'])
    return len(data['entries'])
if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--create',action='store_true');a=p.parse_args()
    if a.create:create()
    print('verified',verify(),'unified software artifacts')
