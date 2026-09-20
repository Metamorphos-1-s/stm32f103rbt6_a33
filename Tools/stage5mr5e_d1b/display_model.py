"""Integer reference models for R5 ACTIVE display hold behavior."""

from dataclasses import dataclass


def round_div(value, divisor):
    sign = -1 if value < 0 else 1
    return sign * ((abs(value) + divisor // 2) // divisor)


def quantize_count(mass_ug, unit_factor_ug, decimal_scale, division):
    count = round_div(mass_ug * decimal_scale, unit_factor_ug)
    return round_div(count, division) * division


@dataclass(frozen=True)
class CandidateConfig:
    hysteresis_divisions: int
    confirmation_ms: int
    large_step_divisions: int = 8


class IncrementalDisplay:
    """ACTIVE-only count-domain candidate with O(1) state."""

    def __init__(self, config):
        self.config = config
        self.reset()

    def reset(self):
        self.display_count = 0
        self.candidate_count = 0
        self.candidate_start_ms = 0
        self.initialized = False
        self.confirmation_count = 0
        self.release_reason = 0
        self.locked = False
        self.catching_up = False

    def process(self, desired_count, now_ms, stable, active, valid=True,
                baseline_count=None):
        if baseline_count is None:
            baseline_count = desired_count
        if not self.initialized or not valid:
            self.display_count = baseline_count
            self.candidate_count = desired_count
            self.candidate_start_ms = now_ms
            self.initialized = True
            self.locked = bool(valid)
            self.catching_up = False
            self.release_reason = 0
            return self.snapshot(desired_count, False)
        if not active:
            self.display_count = baseline_count
            self.candidate_count = desired_count
            self.candidate_start_ms = now_ms
            self.confirmation_count = 0
            self.locked = False
            self.catching_up = False
            self.release_reason = 0
            return self.snapshot(desired_count, False)
        difference = desired_count - self.display_count
        large_step = abs(difference) > self.config.large_step_divisions
        if large_step:
            self.display_count = desired_count
            self.candidate_count = desired_count
            self.candidate_start_ms = now_ms
            self.confirmation_count = 0
            self.locked = False
            self.catching_up = False
            self.release_reason = 2
            return self.snapshot(desired_count, True)
        self.locked = True
        self.release_reason = 0
        if abs(difference) < self.config.hysteresis_divisions:
            self.candidate_count = desired_count
            self.candidate_start_ms = now_ms
            self.confirmation_count = 0
            self.catching_up = False
            return self.snapshot(desired_count, False)
        if self.catching_up:
            self.candidate_count = desired_count
            if now_ms != self.candidate_start_ms:
                self.candidate_start_ms = now_ms
                self.display_count += 1 if difference > 0 else -1
            self.confirmation_count = 0
            self.release_reason = 0
            if self.display_count == desired_count:
                self.catching_up = False
            return self.snapshot(desired_count, False)
        if desired_count != self.candidate_count:
            self.candidate_count = desired_count
            self.candidate_start_ms = now_ms
            self.confirmation_count = 1
            return self.snapshot(desired_count, False)
        self.confirmation_count = min(65535, self.confirmation_count + 1)
        if stable and ((now_ms - self.candidate_start_ms) & 0xFFFFFFFF) >= \
                self.config.confirmation_ms:
            self.display_count += 1 if difference > 0 else -1
            self.confirmation_count = 0
            self.catching_up = self.display_count != desired_count
            if self.catching_up:
                self.candidate_start_ms = now_ms
            self.release_reason = 0
        return self.snapshot(desired_count, False)

    def snapshot(self, desired_count, large_step):
        return {"desired_display_count": desired_count,
            "actual_display_count": self.display_count,
            "anchor_count": self.display_count,
            "locked": int(self.locked),
            "confirmation_count": self.confirmation_count,
            "release_reason": self.release_reason,
            "large_step": int(large_step)}
