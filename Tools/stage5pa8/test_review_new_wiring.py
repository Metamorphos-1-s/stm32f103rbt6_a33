import json,hashlib
from pathlib import Path
p=Path('Results/stage5pa8/review.json'); x=json.loads(p.read_text()); assert not x['sha_match']; assert len(x['sha_expected'])==63; assert x['records']==185910; assert sum(e['kind']=='load' for e in x['edges'])==3; assert sum(e['kind']=='unload' for e in x['edges'])==4
for v in x['candidates'].values(): assert v['max_10s_offset_change_ug'] <= 500
print('STAGE5PA8 REVIEW TEST PASS')

