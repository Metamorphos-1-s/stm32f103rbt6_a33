"""Exercise read-only A9 parser with synthetic non-atomic ADC delays."""
from datetime import datetime,timedelta,timezone
from a9_model_review import FIELDS,direct,edges
from analyze_capture import edge_bracket,quality

def make_data():
    start=datetime(2026,9,27,tzinfo=timezone.utc);rows=[]
    for i in range(2200):
        sec=i/10;loaded=(40<=sec<90 or 130<=sec<180)
        raw=(500000 if loaded else 10000)
        filter_lag=(500000 if 40.3<=sec<90.3 or 130.3<=sec<180.3 else 10000)
        gross=(500000000 if loaded else 0)
        d={'t':i*100_000_000,'utc':(start+timedelta(milliseconds=i*100)).isoformat(),
           'raw_adc':raw,'filtered_raw':filter_lag,'gross_ug':gross,
           'display_count':50000 if loaded else 0,'conditioned_display_ug':gross,'display_anchor_ug':gross,
           'sample_sequence':str(i+1),'mcu_uptime_ms':str(i*100),'fault_mask':'0','overrun_count':'0',
           'dirty':'0','revision':'19','saved_revision':'19','application':'0','mode':'0','offset_ug':'0',
           'save_request_count_low':'10','calibration_valid':'1','official_stable':'1'}
        rows.append(d)
    return rows

def test_parser():
    rows=make_data();ed=edges(rows); assert [v['kind'] for v in ed]==['load','unload','load','unload']
    assert all(0<e['width_s']<=0.1000001 for e in ed)
    q=quality(rows); assert q['estimated_sequence_coverage']==1 and not q['utc_clock_jumps']
    first=edge_bracket(rows,ed[0],'filtered_raw',ed[1]['center_s'])
    assert first['bracket_width_s']<=0.101
    assert first['bracket_monotonic_ns'][1]>ed[0]['hi_s']*1e9
    phases=direct(rows,ed)
    assert phases[1]['checkpoints']['1']['status']=='NOT AVAILABLE'
    assert phases[1]['checkpoints']['45']['status']=='NOT AVAILABLE'
    assert phases[1]['early']['gross_ug'] is None
    assert phases[2]['span_early_g']==500

if __name__=='__main__':
    test_parser();print('A9 CAPTURE ANALYSIS SYNTHETIC PASS')
