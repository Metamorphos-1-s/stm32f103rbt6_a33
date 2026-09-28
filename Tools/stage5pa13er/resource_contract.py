"""Prospective A13E diagnostic identity, retaining A13D-R cumulative rules."""
import sys
from pathlib import Path

sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'stage5pa13dr'))
from contract import Checker as CumulativeChecker, PATHS, decode


class Checker(CumulativeChecker):
    """Validate NEW 0521 build; never relabel the historical A13D-R data."""
    def check(self, target, host_ns, safety):
        if safety['firmware'] != 0x0521 or safety['map'] != 0x010a:
            raise ValueError('wrong A13E diagnostic firmware/map')
        if safety['application'] not in (0,1):
            raise ValueError('invalid ACTIVE application')
        # The R5 application came from an earlier Modbus block than the
        # atomic target mode: never infer a half-switch across two samples.
        # Parent checks unchanged MCU maxima, heartbeat/uptime, sample
        # conservation, RAM, faults, SAVE and revision. Its legacy identity
        # comparison is only an API-specific constant, replaced above; never
        # write the normalized values into the recorded raw snapshot.
        normalized = dict(safety, firmware=0x051f, map=0x0108, application=0)
        return super().check(target, host_ns, normalized)
