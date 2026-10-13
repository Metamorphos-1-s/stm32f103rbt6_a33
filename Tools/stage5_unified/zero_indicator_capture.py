"""Low-load read-only 0x0529 zero-indicator evidence capture."""
import argparse, json, time, csv
from pathlib import Path
from hardware_shadow import SerialTransport, ModbusClient, now, word32, word64, unsigned32

def raw_hex(exchange):
    return dict(tx=exchange.tx.hex().upper(), rx=exchange.rx.hex().upper())

def read(c):
    rt, ex_rt = c.read(0, 32)
    dg, ex_dg = c.read(0x20, 28)
    dc, ex_dc = c.read(0x1e0, 17)
    r5, ex_r5 = c.read(0x280, 40)
    cfg, ex_cfg = c.read(0x100, 64)
    order = 'low' if cfg[3] else 'high'
    return dict(
        utc=now(), host_monotonic_ns=time.monotonic_ns(),
        sample_sequence=unsigned32(dg[0:2], order), mcu_uptime_ms=unsigned32(dg[2:4], order),
        firmware='0x%04X' % rt[15], map='0x%04X' % rt[14],
        gross_ug=word64(rt[20:24], order), net_ug=word64(rt[16:20], order),
        display_count=word32(rt[0:2], order), status_flags=unsigned32(rt[4:6], order),
        official_stable=bool(rt[4] & (1 << 4)), zero_status=bool(rt[4] & (1 << 5)),
        fault_mask=unsigned32(dg[25:27], order), overrun=unsigned32(dg[13:15], order),
        dirty=dg[18], revision=unsigned32(dg[19:21], order), saved_revision=unsigned32(dg[21:23], order),
        active_profile=cfg[31], sample_rate=cfg[32], filter_mode=cfg[34], filter_strength=cfg[35],
        r5_application=r5[1], r5_mode=r5[2], r5_state=r5[3], r5_offset_ug=word64(r5[6:10], order),
        display_condition_state=dc[0], display_locked=bool(dc[1]),
        conditioned_display_ug=word64(dc[2:6], order), display_anchor_ug=word64(dc[6:10], order),
        display_threshold_ug=word64(dc[10:14], order), display_elapsed_ms=unsigned32(dc[14:16], order),
        display_release_reason=dc[16],
        raw_frames=dict(realtime=raw_hex(ex_rt), diagnostics=raw_hex(ex_dg),
                        display_condition=raw_hex(ex_dc), r5=raw_hex(ex_r5), config=raw_hex(ex_cfg)),
    )

def main():
    p=argparse.ArgumentParser(); p.add_argument('--out', type=Path, required=True)
    p.add_argument('--duration', type=float, default=300); p.add_argument('--interval', type=float, default=1.0)
    p.add_argument('--stop-file', type=Path, required=True); a=p.parse_args(); a.out.mkdir(parents=True, exist_ok=True)
    samples=a.out/'samples.jsonl'; errors=a.out/'errors.jsonl'; end=time.monotonic()+a.duration; count=0
    with SerialTransport('COM5',115200,'N',1,350) as port, samples.open('w',encoding='utf-8') as sf, errors.open('w',encoding='utf-8') as ef:
        c=ModbusClient(port,1)
        while time.monotonic()<end and not a.stop_file.exists():
            started=time.monotonic_ns()
            try:
                item=read(c); item['poll_elapsed_ns']=time.monotonic_ns()-started; sf.write(json.dumps(item)+'\n'); sf.flush(); count+=1
            except Exception as exc:
                ef.write(json.dumps(dict(utc=now(), host_monotonic_ns=time.monotonic_ns(), error=str(exc)))+'\n'); ef.flush()
            time.sleep(a.interval)
    (a.out/'summary.json').write_text(json.dumps(dict(samples=count, stopped_by_file=a.stop_file.exists(), duration_s=a.duration, interval_s=a.interval),indent=2)+'\n')

if __name__=='__main__': main()
