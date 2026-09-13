#!/usr/bin/env python3
import argparse, csv, json, statistics
from pathlib import Path

UINT32=1<<32
def delta32(first,last): return (last-first)&0xffffffff
def median_mad(values):
    median=statistics.median(values)
    return median,statistics.median(abs(x-median) for x in values)
def analyze(samples,diagnostics=None):
    with Path(samples).open(encoding='utf-8',newline='') as stream:rows=list(csv.DictReader(stream))
    if len(rows)<2:raise ValueError('at least two samples required')
    seq=[int(x['sample_sequence']) for x in rows];stamp=[int(x['mcu_uptime_ms']) for x in rows];raw=[int(x['raw_adc']) for x in rows]
    sequence_delta=delta32(seq[0],seq[-1]);timestamp_delta=delta32(stamp[0],stamp[-1]);jumps=[delta32(a,b) for a,b in zip(seq,seq[1:])];median,mad=median_mad(raw)
    result={'sequence_delta':sequence_delta,'mcu_timestamp_delta_ms':timestamp_delta,'processed_rate_hz':sequence_delta/(timestamp_delta/1000) if timestamp_delta else None,'host_records':len(rows),'host_coverage_ratio':(len(rows)-1)/sequence_delta if sequence_delta else None,'maximum_sequence_jump':max(jumps),'raw_min':min(raw),'raw_max':max(raw),'raw_peak_to_peak':max(raw)-min(raw),'raw_median':median,'raw_mad':mad,'near_rail_events':sum(abs(x)>=0x700000 for x in raw),'single_sample_jump_events':sum(abs(b-a)>=1000000 for a,b in zip(raw,raw[1:])),'read_error_delta':int(rows[-1].get('read_error_count',0) or 0)-int(rows[0].get('read_error_count',0) or 0),'fifo_overrun_delta':int(rows[-1].get('overrun_count',0) or 0)-int(rows[0].get('overrun_count',0) or 0),'driver_sample_delta':None,'driver_minus_processed':None,'drdy_minus_driver':None}
    if diagnostics:
        d=json.loads(Path(diagnostics).read_text(encoding='utf-8'));before=d['before'];after=d['after'];driver=delta32(before['driver_sample_count'],after['driver_sample_count']);processed=delta32(before['processed_sample_count'],after['processed_sample_count']);result['driver_sample_delta']=driver;result['driver_minus_processed']=driver-processed;result['read_error_delta']=delta32(before['read_error_count'],after['read_error_count']);result['fifo_overrun_delta']=delta32(before['fifo_overrun_count'],after['fifo_overrun_count']);result['drdy_minus_driver']=None if 'drdy_count' not in after else delta32(before['drdy_count'],after['drdy_count'])-driver
    return result
def main():
    p=argparse.ArgumentParser();p.add_argument('--samples',required=True);p.add_argument('--diagnostics');p.add_argument('--output',required=True);a=p.parse_args();result=analyze(a.samples,a.diagnostics);Path(a.output).write_bytes((json.dumps(result,indent=2)+'\n').encode('utf-8'));print(json.dumps(result));return 0
if __name__=='__main__':raise SystemExit(main())
