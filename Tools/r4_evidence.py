import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];BASE=ROOT/'Results/stage5pa13er4';MANIFEST=BASE/'manifest.json'
def selected():
 p={x for x in BASE.rglob('*') if x.is_file() and x!=MANIFEST and x.suffix!='.pyc'}
 p.update((ROOT/'Docs').glob('STAGE5PA13ER4*.md'));p.update((ROOT/'Tools').glob('r4_*.py'));p.update((ROOT/'Tools').glob('test_r4_*.py'));return sorted(p)
def create():
 e=[]
 for p in selected():
  d=p.read_bytes();e.append(dict(path=p.relative_to(ROOT).as_posix(),bytes=len(d),sha256=hashlib.sha256(d).hexdigest().upper()))
 MANIFEST.write_text(json.dumps(dict(schema=1,stage='A13E-R4',baseline='9b2e4baa17fb698ff3f3b3239690bb1902665e51',classification='H1_NONZERO_RAW_ATOMIC_PASS',entries=e),indent=2)+'\n')
def verify():
 e=json.loads(MANIFEST.read_text())['entries']
 for x in e:
  p=ROOT/x['path'];d=p.read_bytes();assert len(d)==x['bytes'] and hashlib.sha256(d).hexdigest().upper()==x['sha256'],x['path']
 return len(e)
if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--create',action='store_true');a=p.parse_args()
 if a.create:create()
 print('verified',verify(),'R4 artifacts')
