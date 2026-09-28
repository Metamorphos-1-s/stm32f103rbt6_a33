"""Prospective target-cumulative contract; not the A13D historical rule."""
PATHS = ('OFF','HOLDOFF','REFERENCE_FILL','OBSERVATION_FILL','TRACKING',
         'FAST_TRACKING','STEP_PENDING','STEP_SETTLING','DOSING','INVALID')
SIGNATURE = 0xA13D5201


def decode(words):
    if len(words) != 122:
        raise ValueError('wrong atomic snapshot length')
    def u32(first):
        return (words[first] << 16) | words[first+1]
    m = [u32(i*2) for i in range(32)]
    maxima = [u32(64+i*2) for i in range(10)]
    counts = words[84:94]
    peaks = [u32(94+i*2) for i in range(10)]
    tail = [u32(114+i*2) for i in range(4)]
    return dict(signature=m[0], generation_begin=m[1], generation_end=tail[3],
        hclk_hz=m[2], flags=m[3]&255, overhead_cycles=m[3]>>8,
        first=m[4], last=m[5], total=m[6], max_cycles=m[7], peak_seq=m[8], peak_path=m[9],
        loop_max=m[10], interval_max=m[11], loops=m[12], touched=m[13], static_end=m[14],
        stack_top=m[15], control=m[16], now_ms=m[17], produced=m[18], consumed=m[19],
        invalid=m[20], fifo=m[21], read_errors=m[22], overrun=m[23], engine=m[24],
        sample_ms=m[25], mode=m[26], state=m[27], gates=m[28], boost_samples=m[29],
        coverage=m[30], version=m[31], maxima=maxima, counts=counts, peaks=peaks,
        fault=tail[0], leased=tail[1], driver_state=tail[2])


class Checker:
    def __init__(self):
        self.previous = None
        self.qualified = False
        self.gaps = []
        self.backlog_since = None

    def check(self, s, host_ns, safety):
        if s['signature'] != SIGNATURE or s['version'] != 1:
            raise ValueError('wrong contract signature/version')
        if s['generation_begin'] != s['generation_end'] or s['generation_begin'] & 1:
            raise ValueError('incoherent atomic snapshot')
        if s['flags'] != 1 or not s['leased'] or s['control'] & 2:
            raise ValueError('invalid target diagnostics/lease/MSP')
        if s['touched']-s['static_end'] < 512:
            raise ValueError('runtime stack margin below512')
        if safety['firmware'] != 0x051f or safety['map'] != 0x0108:
            raise ValueError('identity changed')
        if any(s[k] for k in ('fault','overrun','read_errors','invalid')) or safety['dirty'] or safety['application']:
            raise ValueError('target fault/error/dirty/ACTIVE')
        if safety['revision'] != 19 or safety['saved_revision'] != 19 or safety['save_count']:
            raise ValueError('persistence invariant')
        if sum(s['counts']) != s['total'] or s['counts'][9]:
            raise ValueError('target call counter mismatch')
        expected_mask = sum(1<<i for i,c in enumerate(s['counts']) if c)
        if expected_mask != s['coverage']:
            raise ValueError('target coverage mismatch')
        if s['total']:
            if not s['first'] or s['last']-s['first']+1 != s['total']:
                raise ValueError('target timed sample loss')
            if s['max_cycles'] != max(s['maxima']) or not 0 <= s['peak_path'] < 9:
                raise ValueError('target peak inconsistency')
            p = s['peak_path']
            if s['maxima'][p] != s['max_cycles'] or s['peaks'][p] != s['peak_seq']:
                raise ValueError('target peak identity inconsistency')
        for count,cycles,seq in zip(s['counts'],s['maxima'],s['peaks']):
            if count and (cycles == 0 or not s['first'] <= seq <= s['last']):
                raise ValueError('path peak missing')
            if not count and (cycles or seq):
                raise ValueError('path peak without calls')
        hclk=s['hclk_hz']
        if hclk<1000000 or max(s['maxima'])*100>hclk:
            raise ValueError('target call exceeds10ms')
        if s['loop_max']*40>hclk or s['interval_max']*40>hclk:
            raise ValueError('target loop exceeds25ms')
        if s['produced']-s['consumed'] != s['fifo'] or s['engine'] != s['consumed']:
            raise ValueError('target sample/FIFO conservation')
        if s['last'] != s['engine']:
            raise ValueError('every Feed not accounted for')
        if s['fifo']>1:
            self.backlog_since = self.backlog_since or host_ns
            if host_ns-self.backlog_since>1000000000:
                raise ValueError('sustained FIFO backlog')
        else:
            self.backlog_since=None
        if not self.qualified:
            if s['now_ms']>=5000 and s['driver_state']==4 and safety['calibration_valid']:
                self.qualified=True
            else:
                return
        if s['now_ms']>3600000:
            raise ValueError('one-hour evidence horizon exceeded')
        if self.previous:
            old,t = self.previous
            dt=(host_ns-t)/1000000
            if dt>5000:
                raise ValueError('heartbeat absence INCOMPLETE')
            if s['now_ms']<old['now_ms'] or abs((s['now_ms']-old['now_ms'])-dt)>250:
                raise ValueError('MCU restart/clock inconsistency')
            for key in ('generation_begin','last','total','loops','loop_max','interval_max'):
                if s[key]<old[key]:
                    raise ValueError('target accumulator reset')
            for key in ('maxima','counts'):
                if any(a<b for a,b in zip(s[key],old[key])):
                    raise ValueError('target per-path evidence reset')
            if s['touched']>old['touched']:
                raise ValueError('target watermark reset')
            delta=s['last']-old['last']
            if delta>1:
                self.gaps.append(dict(previous=old['last'],current=s['last'],
                    calls_not_seen_as_individual_polls=delta-1,
                    note='retained in MCU maxima/counts; not interpolated'))
        self.previous = (s,host_ns)

    def complete(self):
        if not self.previous:
            return 'INCOMPLETE'
        s,_=self.previous
        return 'PASS' if s['coverage'] & 511 == 511 else 'INCOMPLETE'
