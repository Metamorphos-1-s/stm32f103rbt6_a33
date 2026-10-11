"""Recompute and audit the R4/R5 cross-record resource evidence."""
import csv
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PATHS = ('OFF', 'HOLDOFF', 'REFERENCE_FILL', 'OBSERVATION_FILL',
         'TRACKING', 'FAST_TRACKING', 'STEP_PENDING', 'STEP_SETTLING', 'DOSING')
MASK = {name: 1 << i for i, name in enumerate(PATHS)}


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest().upper()


def audit_run(name, directory, expected_epoch):
    snap = json.loads((directory / 'final_target_snapshot.json').read_text())
    target = snap['target']
    rows = list(csv.DictReader((directory / 'polls.csv').open(encoding='utf-8')))
    maxima = target['maxima']; counts = target['counts']; peaks = target['peaks']
    recomputed = []
    for i, path in enumerate(PATHS):
        field = path + '_max_cycles'
        row_max = max((int(row.get(field, 0) or 0) for row in rows), default=0)
        recomputed.append(row_max)
        if row_max != maxima[i]:
            raise ValueError('%s %s CSV/final mismatch: %s != %s' %
                             (name, path, row_max, maxima[i]))
    modes = sorted(set(int(row['mode']) for row in rows if row.get('mode') not in ('', None)))
    return dict(name=name, epoch=target['epoch_id'], epoch_matches=(target['epoch_id'] == expected_epoch),
                rows=len(rows), firmware=snap['safety']['firmware'], map=snap['safety']['map'],
                hclk_hz=target['hclk_hz'], modes=modes, maxima=maxima, counts=counts,
                app_run_max=target['loop_max'], loop_interval_max=target['interval_max'],
                peaks=peaks, coverage=target['coverage'], flags=target['flags'],
                stack_untouched=target['touched'] - target['static_end'],
                produced=target['produced'], consumed=target['consumed'], engine=target['engine'],
                fifo=target['fifo'], fault=target['fault'], overrun=target['overrun'],
                read_errors=target['read_errors'], dirty=snap['safety']['dirty'],
                revision=snap['safety']['revision'], saved_revision=snap['safety']['saved_revision'],
                save_count=snap['safety']['save_count'], host_gaps=json.loads(
                    (directory / 'summary.json').read_text())['host_gap_events'],
                snapshots_sha256=digest(directory / 'snapshots.jsonl'),
                polls_sha256=digest(directory / 'polls.csv'))


def main():
    r4_dir = ROOT / 'Results/stage5p_unified/hardware/stage5pr4_20261011/resource_capture_r4b'
    r5_dir = ROOT / 'Results/stage5p_unified/hardware/stage5r_rerun_20261011/resource_capture_final'
    r4 = audit_run('R4', r4_dir, 351272961)
    r5 = audit_run('R5', r5_dir, 348651521)
    per_path = []
    for i, path in enumerate(PATHS):
        sources = []
        for run in (r4, r5):
            if run['counts'][i]:
                directory = r4_dir if run['name'] == 'R4' else r5_dir
                rows = list(csv.DictReader((directory / 'polls.csv').open(encoding='utf-8')))
                mode_values = []
                count_field = path + '_count'
                prior = 0
                for row in rows:
                    current = int(row.get(count_field, 0) or 0)
                    if current > prior:
                        mode_values.append(int(row['mode']))
                    prior = current
                sources.append(dict(run=run['name'], mode_values=run['modes'],
                                    first_call_mode_values=sorted(set(mode_values)),
                                    epoch=run['epoch'], calls=run['counts'][i],
                                    peak_cycles=run['maxima'][i], peak_sequence=run['peaks'][i]))
        per_path.append(dict(path=path, bit=MASK[path], sources=sources,
                             union_evidence=bool(sources)))
    combined = dict(coverage=r4['coverage'] | r5['coverage'], paths=per_path,
                    max_feed=max(max(r4['maxima']), max(r5['maxima'])),
                    max_app_run=max(r4['app_run_max'], r5['app_run_max']),
                    max_loop_interval=max(r4['loop_interval_max'], r5['loop_interval_max']),
                    same_diagnostic=(r4['firmware'], r4['map']) == (r5['firmware'], r5['map']) == (1318, 270),
                    safe_runs=all(r['flags'] == 1 and r['stack_untouched'] >= 512 and
                                  r['produced'] == r['consumed'] and r['fifo'] == 0 and
                                  r['fault'] == r['overrun'] == r['read_errors'] == 0 and
                                  r['save_count'] == 0 for r in (r4, r5)),
                    combined_qualification=(r4['coverage'] | r5['coverage']) == 0x1FF)
    out = dict(runs=[r4, r5], combined=combined)
    Path('Results/stage5p_unified/audit').mkdir(parents=True, exist_ok=True)
    Path('Results/stage5p_unified/audit/cross_record_resource_audit.json').write_text(
        json.dumps(out, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(out, indent=2))


if __name__ == '__main__':
    main()
