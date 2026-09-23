"""Reference model for the Stage 5M-R5E-D1-D unified display conditioner."""

from dataclasses import dataclass


FACTORS = {0: 1_000_000_000, 1: 1_000_000, 2: 453_592_370}
TRACKING, CANDIDATE, LOCKED = range(3)
NONE, UNSTABLE, DEVIATION, OVERLOAD, CALIBRATION, NOT_ALLOWED, FORCED, \
    SOURCE_CHANGE, LARGE_STEP, SLOW_FOLLOW, INVALID_DOMAIN = range(11)


def round_div(value, divisor):
    sign = -1 if value < 0 else 1
    return sign * ((abs(value) + divisor // 2) // divisor)


def mass_to_count(mass_ug, unit, decimals, division):
    if unit not in FACTORS or decimals > 5 or division not in (1, 2, 5):
        return None
    raw = round_div(mass_ug * (10 ** decimals), FACTORS[unit])
    return round_div(raw, division) * division


def count_to_mass(count, unit, decimals):
    if unit not in FACTORS or decimals > 5:
        return None
    return round_div(count * FACTORS[unit], 10 ** decimals)


@dataclass
class UnifiedInput:
    mass_ug: int
    sequence: int
    now_ms: int
    source: int = 0
    unit: int = 1
    decimals: int = 2
    division: int = 1
    stable: bool = True
    valid: bool = True
    reset: bool = False
    operator_zero: bool = False


class UnifiedDisplay:
    def __init__(self):
        self.state = TRACKING
        self.displayed = 0
        self.desired = 0
        self.anchor = 0
        self.direction = 0
        self.evidence = 0
        self.source = 0
        self.release_reason = NONE
        self.locked = False
        self.domain_valid = False
        self.large_step = False
        self.operator_zero = False
        self.operator_start_ms = 0
        self.candidate_start_ms = 0
        self.samples = []
        self.last_sequence = 0
        self.last_evidence_ms = 0
        self.have_evidence_time = False
        self.release_samples = 0

    @staticmethod
    def sign(value):
        return 1 if value > 0 else -1 if value < 0 else 0

    def force_tracking(self, reason):
        self.state = TRACKING
        self.locked = False
        self.operator_zero = False
        self.release_reason = reason
        self.domain_valid = False
        self.direction = 0
        self.evidence = 0
        self.large_step = False
        self.have_evidence_time = False
        self.samples = []
        self.release_samples = 0

    def request_zero(self, now_ms):
        self.samples = []
        self.release_samples = 0
        self.state = LOCKED
        self.displayed = 0
        self.desired = 0
        self.anchor = 0
        self.locked = True
        self.operator_zero = True
        self.domain_valid = True
        self.release_reason = NONE
        self.direction = 0
        self.evidence = 0
        self.large_step = False
        self.operator_start_ms = now_ms

    def force_domain(self, item, desired, reason):
        self.force_tracking(reason)
        self.displayed = desired
        self.desired = desired
        self.source = item.source
        self.domain_valid = True
        self.last_sequence = item.sequence

    def process(self, item):
        if item.operator_zero:
            self.request_zero(item.now_ms)
        desired = mass_to_count(item.mass_ug, item.unit, item.decimals,
            item.division)
        if item.reset:
            if desired is None:
                self.force_tracking(FORCED)
            else:
                self.force_domain(item, desired, FORCED)
            return self.snapshot(item)
        if not item.valid:
            if desired is None:
                self.force_tracking(NOT_ALLOWED)
            else:
                self.force_domain(item, desired, NOT_ALLOWED)
            return self.snapshot(item)
        if desired is None or not -(1 << 31) <= desired < (1 << 31):
            self.force_tracking(INVALID_DOMAIN)
            return self.snapshot(item)
        self.desired = desired
        if not self.domain_valid or self.source != item.source:
            self.force_tracking(SOURCE_CHANGE)
            self.displayed = desired
            self.desired = desired
            self.source = item.source
            self.domain_valid = True
            self.last_sequence = item.sequence
            return self.snapshot(item)

        if self.state == TRACKING:
            self.displayed = desired
            self.last_sequence = item.sequence
            self.direction = self.evidence = 0
            self.large_step = False
            if item.stable:
                self.samples = [desired]
                self.candidate_start_ms = item.now_ms
                self.state = CANDIDATE
            return self.snapshot(item)

        if self.state == CANDIDATE:
            self.displayed = desired
            if not item.stable:
                self.force_domain(item, desired, UNSTABLE)
                return self.snapshot(item)
            if item.sequence != self.last_sequence:
                self.samples.append(desired)
                self.samples = self.samples[-9:]
                self.last_sequence = item.sequence
            if item.now_ms - self.candidate_start_ms >= 500 and \
                    len(self.samples) >= 9:
                self.displayed = sorted(self.samples)[4]
                self.anchor = self.displayed
                self.state = LOCKED
                self.locked = True
                self.direction = self.evidence = 0
                self.large_step = False
                self.release_samples = 0
            return self.snapshot(item)

        if not item.stable and (not self.operator_zero or
                item.now_ms - self.operator_start_ms >= 3000):
            self.force_domain(item, desired, UNSTABLE)
            return self.snapshot(item)

        if not self.operator_zero:
            delta = desired - self.displayed
            magnitude = abs(delta)
            direction = self.sign(delta)
            self.direction = direction
            self.large_step = False
            if magnitude > item.division * 8:
                self.displayed = desired
                self.anchor = desired
                self.release_reason = LARGE_STEP
                self.large_step = True
                self.evidence = 0
                self.last_sequence = item.sequence
                return self.snapshot(item)
            if item.sequence != self.last_sequence:
                self.last_sequence = item.sequence
                if not self.have_evidence_time or \
                        ((item.now_ms - self.last_evidence_ms) &
                         0xFFFFFFFF) >= 100:
                    self.last_evidence_ms = item.now_ms
                    self.have_evidence_time = True
                    if magnitude <= item.division:
                        self.evidence -= self.sign(self.evidence)
                    elif item.stable:
                        if self.evidence == 0 or self.sign(self.evidence) == direction:
                            self.evidence = max(-6, min(6,
                                self.evidence + direction))
                        else:
                            self.evidence += direction
            if magnitude > item.division and \
                    self.sign(self.evidence) == direction and \
                    abs(self.evidence) >= 5:
                self.displayed += direction * item.division
                self.anchor = self.displayed
                self.release_reason = SLOW_FOLLOW
                self.evidence = 0
            return self.snapshot(item)

        division_mass = count_to_mass(item.division, item.unit, item.decimals)
        if division_mass is None:
            self.force_tracking(INVALID_DOMAIN)
            return self.snapshot(item)
        if abs(item.mass_ug) > 8 * division_mass:
            self.release_samples += 1
            if self.release_samples >= 3:
                self.force_domain(item, desired, DEVIATION)
        else:
            self.release_samples = 0
        return self.snapshot(item)

    def snapshot(self, item):
        return {
            "desired": self.desired,
            "displayed": self.displayed,
            "delta": self.desired - self.displayed,
            "state": self.state,
            "anchor": self.displayed,
            "direction": self.direction,
            "evidence": self.evidence,
            "source": self.source,
            "release_reason": self.release_reason,
            "locked": int(self.locked),
            "stable": int(item.stable),
            "valid": int(item.valid),
            "large_step": int(self.large_step),
            "sample_sequence": self.last_sequence,
            "operator_zero": int(self.operator_zero),
        }
