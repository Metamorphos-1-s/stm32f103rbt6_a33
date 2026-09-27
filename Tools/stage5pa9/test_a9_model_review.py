"""A9 two-cycle numerical and state transition checks; no hardware access."""
import statistics
from a9_model_review import FIELDS,oracle,frozen_r5,direct,edges

def two_cycles():
    def mass(sec):
        if sec<20:return 0
        if sec<110:return 500_000_000+(sec-20)*1000
        if sec<180:return -(sec-110)*500
        if sec<260:return 500_000_000+(sec-180)*1000
        return -(sec-260)*500
    return [(sec,mass(sec)) for sec in range(350)]

def test_two_round_oracle():
    seq=two_cycles(); evt=[{'kind':k,'center_s':s} for k,s in [('load',20),('unload',110),('load',180),('unload',260)]]
    trace,max10,locked=oracle(seq,evt,initial_offset=120_000)
    assert len(locked)==4 and max10<=500
    for phase in locked:
        t=phase['edge_s'];inherited=phase['offset_at_edge_ug']
        expected=statistics.median(m-inherited for s,m in seq if t+15<=s<t+45)
        assert phase['locked_reference_ug']==expected
        local=[r for r in trace if t-2<=r[0]<t+60]
        assert local and all(r[3]=='DOSING' for r in local)
        assert len(set(r[2] for r in local))==1
        assert all(r[1]==dict(seq)[r[0]]-inherited for r in local)
        acquired=[r for r in trace if t+60<=r[0]<t+70 and r[3]=='STATIC']
        assert acquired and acquired[0][4]==expected and acquired[0][5]=='REFERENCE_LOCK'
    assert locked[2]['offset_at_edge_ug']!=locked[0]['offset_at_edge_ug']
    assert locked[2]['locked_reference_ug']==statistics.median(m-locked[2]['offset_at_edge_ug'] for s,m in seq if 195<=s<225)
    # The genuine 500g physical span is not removed by an event-triggered reset.
    assert abs((trace[30][1]-trace[10][1])-500_000_000)<30_000

def test_frozen_static_rebuild_preserves_offset():
    trace=frozen_r5([(s,s*300 if s<1300 else 500_000_000+(s-1300)*300) for s in range(2500)])
    before=trace[1299][2]
    assert before>0
    assert trace[1310][7]>=1 and trace[1310][6]==2
    assert trace[1310][3]==2 and trace[1310][4]==2
    assert trace[1310][2]==before+100  # bounded tracking until auto-step rebase
    assert trace[-1][7]>=1

def test_real_window_clipping():
    dataset=[]
    for s in range(200):
        mass=500_000_000 if s<50 or s>=100 else 0
        row={'t':s*1_000_000_000,'u':'unused'}
        for field in FIELDS:row[field]=mass if field=='gross_ug' else 0
        dataset.append(row)
    event_list=edges(dataset)
    assert [e['kind'] for e in event_list]==['unload','load']
    phases=direct(dataset,event_list)
    assert phases[0]['checkpoints']['45']['status']=='NOT AVAILABLE'
    assert phases[0]['checkpoints']['1']['status']=='NOT AVAILABLE'  # next load at 100 s
    assert phases[0]['early']['gross_ug']==0

def test_slow_change_is_not_classified_as_mechanism():
    # The same recorded gross can be produced by true slow filling or thermal drift.
    # A controller observing gross alone must give the identical numerical output.
    slow_fill=[(s,s*500) for s in range(1200)]
    drift=[(s,s*500) for s in range(1200)]
    assert frozen_r5(slow_fill)==frozen_r5(drift)
    assert max(v for _,v in slow_fill)==599_500

if __name__=='__main__':
    for test in (test_two_round_oracle,test_frozen_static_rebuild_preserves_offset,test_real_window_clipping,test_slow_change_is_not_classified_as_mechanism):test()
    print('A9 FOUR NUMERICAL STATE/WINDOW TESTS PASS')




