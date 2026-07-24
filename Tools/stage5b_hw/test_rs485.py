"""RS485 test wrapper."""

from test_smoke import run as run_smoke
from test_protocol_errors import run as run_errors


def run(client, report, count=100, interval_s=0.05, include_errors=False):
    print("Set the board switch to RS485. Connect A, B and GND; swap A/B if no response.")
    ok = run_smoke(client, report, count, interval_s)
    if ok and include_errors:
        ok = run_errors(client, report)
    elif ok:
        report.add("protocol error suite", "SKIPPED",
                   "use --include-errors --allow-write to enable malformed FC06/FC16 cases")
    report.add("RS485 electrical timing", "REQUIRES LOGIC ANALYZER",
               "DE release, t1.5/t3.5 and A/B signal integrity are not measured")
    return ok
