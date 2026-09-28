"""Offline audit of the ABORTED A13E-R resource recording; no reclassification."""
import argparse
import hashlib
import json
from pathlib import Path
from resource_contract import Checker,PATHS


def evaluate(directory):
    outcome=json.loads((directory/'summary.json').read_text())
    checker=Checker();first=last=None;polls=0
    for text in (directory/'snapshots.jsonl').open(encoding='utf-8'):
        record=json.loads(text)
        checker.check(record['target'],record['host_monotonic_ns'],record['safety'])
        first=first or record;last=record;polls+=1
    if not last or outcome['status']!='FAIL' or polls!=outcome['polls']:
        raise ValueError('expected original aborted evidence')
    before=directory.parent/'backup';after=directory/'restore'
    app_before=(before/'application.bin').read_bytes();app_after=(after/'application_after.bin').read_bytes()
    cfg_before=(before/'config_region.bin').read_bytes();cfg_after=(after/'config_after.bin').read_bytes()
    final=last['target'];hclk=final['hclk_hz']
    return dict(classification='HISTORICAL_ABORTED_RECORD_NOT_RESOURCE_PASS',
        original_status=outcome['status'],original_error=outcome['error'],
        host_polls=polls,first_utc=first['utc'],last_utc=last['utc'],
        monotonic_span_s=(last['host_monotonic_ns']-first['host_monotonic_ns'])/1e9,
        host_gap_events=checker.gaps,
        resource_coverage=final['coverage'],required_coverage=511,
        path_calls=dict(zip(PATHS,final['counts'])),
        target_first=final['first'],target_last=final['last'],target_produced=final['produced'],
        target_consumed=final['consumed'],target_engine=final['engine'],target_fifo=final['fifo'],
        feed_peak_cycles=final['max_cycles'],feed_peak_ms=final['max_cycles']*1000/hclk,
        loop_peak_ms=final['loop_max']*1000/hclk,
        loop_interval_peak_ms=final['interval_max']*1000/hclk,
        observed_untouched_ram_bytes=final['touched']-final['static_end'],
        last_observed_shadow_offset_ug=last['original']['beta_offset_ug'],
        target_fault=final['fault'],target_overrun=final['overrun'],read_errors=final['read_errors'],
        recovery_full_application_equal=app_before==app_after,
        recovery_config_equal=cfg_before==cfg_after,
        restored_application_sha256=hashlib.sha256(app_after).hexdigest().upper(),
        restored_config_sha256=hashlib.sha256(cfg_after).hexdigest().upper(),
        qualification='INCOMPLETE; boot/empty STATIC only, resource recorder aborted on ACTIVE admission',
        no_active_bin_flashed=True)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--input',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True);a=p.parse_args()
    result=evaluate(a.input);a.output.write_text(json.dumps(result,indent=2)+'\n')
    print(json.dumps({k:v for k,v in result.items() if k not in ('host_gap_events','path_calls')}))
