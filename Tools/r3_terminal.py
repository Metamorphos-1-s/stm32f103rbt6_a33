"""Read-only R3 post-power terminal with complete request/response archive."""
import argparse
import json
from pathlib import Path
from r3_raw_closure import LoggedClient,SerialTransport,probe,snapshot,healthy,i64

def capture(out):
    if out.exists():raise ValueError('existing terminal directory; no overwrite')
    out.mkdir(parents=True)
    with (out/'frames.jsonl').open('x') as frames:
        with SerialTransport('COM5',115200,'N',1,350) as transport:
            client=LoggedClient(transport,frames)
            device=probe(client);s=snapshot(client);healthy(s)
            reference,_=client.read(0x292,4)
    a=s['atomic']
    evidence=dict(device=device,snapshot=s,reference_words=reference,reference_ug=i64(reference,0),
        no_device_write=True,no_swd=True,no_save=True)
    (out/'terminal.json').write_text(json.dumps(evidence,indent=2)+'\n',encoding='utf-8')
    config=device['configuration']
    if (a['application'] or a['mode'] or a['apply'] or a['candidate_offset_ug'] or a['applied_offset_ug']
        or s['checkweigh_words'][1] or evidence['reference_ug'] or
        (config['sample_rate'],config['filter_mode'],config['filter_strength'])!=(0,1,3) or
        (config['raw_zero'],config['raw_span'],config['span_mass_ug'])!=(41868,485780,500000000)):
        raise ValueError('terminal/profile/calibration unsafe or unexplained difference')
    print(json.dumps(dict(utc=device['utc'],firmware=device['firmware'],map=device['map'],
        dirty=device['dirty'],revision=device['revision'],saved=device['saved_revision'],
        fault=device['fault_mask'],overrun=device['overrun_count'],save=s['save_count'],
        application=a['application'],mode=a['mode'],offset=a['candidate_offset_ug'],
        reference_ug=evidence['reference_ug'],apply=a['apply'],checkweigh=s['checkweigh_words'][1])))

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--out',type=Path,required=True)
    capture(p.parse_args().out)
