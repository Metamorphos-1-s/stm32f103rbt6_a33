import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'Results/stage5pa13er2';MANIFEST=BASE/'manifest.json'
paths=set(p for p in BASE.rglob('*') if p.is_file() and p!=MANIFEST)
paths.add(ROOT/'Docs/STAGE5PA13ER2_CONTROLLED_ACTIVE_CLOSURE.md')
paths.add(ROOT/'Tools/stage5pa13er2_active.py')
entries=[]
for p in sorted(paths):
 d=p.read_bytes(); entries.append(dict(path=p.relative_to(ROOT).as_posix(),bytes=len(d),sha256=hashlib.sha256(d).hexdigest().upper()))
MANIFEST.write_text(json.dumps(dict(schema=1,classification='R2_RESOURCE_PASS_ACTIVE_FUNCTIONAL_CLOSURE',baseline='10f43c320b4a4d886aa6e6171f70eb9b0bb0a8b2',entries=entries),indent=2)+'\n')
for e in entries:
 p=ROOT/e['path']; d=p.read_bytes(); assert len(d)==e['bytes'] and hashlib.sha256(d).hexdigest().upper()==e['sha256']
print('verified',len(entries),'R2 artifacts')
