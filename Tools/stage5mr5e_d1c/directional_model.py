"""Fixed-point reference model for directional-evidence display following."""

from dataclasses import dataclass


INT32_MIN = -(1 << 31)
INT32_MAX = (1 << 31) - 1


def round_div(value, divisor):
    sign = -1 if value < 0 else 1
    return sign * ((abs(value) + divisor // 2) // divisor)


def quantize_count(mass_ug, unit_factor_ug, decimal_scale, division):
    count = round_div(mass_ug * decimal_scale, unit_factor_ug)
    return round_div(count, division) * division


@dataclass(frozen=True)
class DirectionalConfig:
    increment: int
    zero_leak: int
    reverse_cancel: int
    threshold: int
    retain_remainder: bool
    large_step_divisions: int = 8


class DirectionalDisplay:
    """O(1) signed-evidence follower; all values are display counts."""

    def __init__(self, config):
        self.config = config
        self.reset()

    def reset(self):
        self.display_count = 0
        self.evidence = 0
        self.initialized = False
        self.locked = False
        self.source = 0
        self.release_reason = 0

    @staticmethod
    def direction(delta):
        return 1 if delta > 0 else -1 if delta < 0 else 0

    def _accumulate(self, direction):
        if direction == 0:
            if self.evidence > 0:
                self.evidence = max(0, self.evidence - self.config.zero_leak)
            elif self.evidence < 0:
                self.evidence = min(0, self.evidence + self.config.zero_leak)
        elif self.evidence == 0 or self.direction(self.evidence) == direction:
            limit = self.config.threshold + self.config.increment
            self.evidence = max(-limit, min(limit,
                self.evidence + direction * self.config.increment))
        else:
            self.evidence += direction * min(abs(self.evidence),
                self.config.reverse_cancel)

    def process(self, desired_count, stable, active, valid=True,
                baseline_count=None, source=0):
        if baseline_count is None:
            baseline_count = desired_count
        if not self.initialized or not valid or self.source != source:
            self.display_count = baseline_count
            self.evidence = 0
            self.initialized = True
            self.locked = bool(valid)
            self.source = source
            self.release_reason = 0
            return self.snapshot(desired_count, stable, active, False)
        if not active:
            self.display_count = baseline_count
            self.evidence = 0
            self.locked = False
            self.release_reason = 0
            return self.snapshot(desired_count, stable, active, False)
        delta = int(desired_count) - int(self.display_count)
        if abs(delta) > self.config.large_step_divisions:
            self.display_count = desired_count
            self.evidence = 0
            self.locked = False
            self.release_reason = 2
            return self.snapshot(desired_count, stable, active, True)
        self.locked = True
        self.release_reason = 0
        direction = self.direction(delta)
        if stable:
            self._accumulate(direction)
        elif direction == 0:
            self._accumulate(0)
        if direction and self.direction(self.evidence) == direction and \
                abs(self.evidence) >= self.config.threshold:
            self.display_count += direction
            if self.config.retain_remainder:
                self.evidence -= direction * self.config.threshold
            else:
                self.evidence = 0
        return self.snapshot(desired_count, stable, active, False)

    def snapshot(self, desired_count, stable, active, large_step):
        delta = int(desired_count) - int(self.display_count)
        return {
            "desired_count": int(desired_count),
            "current_count": int(self.display_count),
            "delta_count": delta,
            "direction": self.direction(delta),
            "evidence": int(self.evidence),
            "anchor_count": int(self.display_count),
            "locked": int(self.locked),
            "stable": int(bool(stable)),
            "active": int(bool(active)),
            "large_step": int(bool(large_step)),
            "release_reason": int(self.release_reason),
            "source": int(self.source),
        }
