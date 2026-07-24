"""RS232 test wrapper."""

from test_smoke import run as run_smoke
from test_protocol_errors import run as run_errors


def run(client, report, count=100, interval_s=0.05, include_errors=False):
    print("Set the board switch to RS232. Use a true USB-RS232 adapter, not USB-TTL.")
    if not run_smoke(client, report, count, interval_s):
        return False
    if include_errors:
        return run_errors(client, report)
    report.add("protocol error suite", "SKIPPED",
               "use --include-errors --allow-write to enable malformed FC06/FC16 cases")
    return True
