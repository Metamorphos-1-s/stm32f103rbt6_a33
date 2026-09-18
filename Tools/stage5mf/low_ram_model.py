"""Stage 5M-F low-RAM adaptive filter reference model."""

from dataclasses import dataclass


PRECISION = 0
FAST = 1
SETTLING = 2

REASON_NONE = 0
REASON_MOTION = 1
REASON_PROCESS_ACTIVE = 2
REASON_QUIET = 3
REASON_CONVERGED = 4
REASON_RESET = 5
REASON_GAP = 6


def divpow(value, shift):
    magnitude = abs(value)
    rounded = (magnitude + (1 << (shift - 1))) >> shift
    return -rounded if value < 0 else rounded


@dataclass(frozen=True)
class Config:
    fast_shift: int = 1
    motion_shift: int = 3
    output_shift: int = 1
    innovation_threshold_ug: int = 50000
    motion_threshold_ug: int = 10000
    settle_threshold_ug: int = 10000
    quiet_samples: int = 3
    stable_samples: int = 5


class LowRamAdaptive:
    """O(1) model matching a <=48-byte fixed-point C state layout."""

    def __init__(self, config=Config()):
        self.config = config
        self.reset()

    def reset(self):
        self.fast_ug = 0
        self.output_ug = 0
        self.previous_input_ug = 0
        self.motion_ewma_ug = 0
        self.last_timestamp_ms = 0
        self.last_sequence = 0
        self.transition_count = 0
        self.dwell = 0
        self.quiet = 0
        self.state = SETTLING
        self.reason = REASON_RESET
        self.initialized = False

    def process(self, raw_mass_ug, precision_mass_ug, timestamp_ms,
                sequence, process_active=False, valid=True):
        if not valid:
            self.reset()
            return self._result(precision_mass_ug, process_active, fault=True)
        if not self.initialized:
            self.fast_ug = raw_mass_ug
            self.output_ug = precision_mass_ug
            self.previous_input_ug = raw_mass_ug
            self.last_timestamp_ms = timestamp_ms
            self.last_sequence = sequence
            self.initialized = True
            if process_active:
                self.state = FAST
                self.reason = REASON_PROCESS_ACTIVE
            return self._result(precision_mass_ug, process_active)
        gap = ((sequence - self.last_sequence) & 0xFFFFFFFF) != 1 or \
              ((timestamp_ms - self.last_timestamp_ms) & 0xFFFFFFFF) > 250
        difference = raw_mass_ug - self.previous_input_ug
        self.motion_ewma_ug += divpow(difference - self.motion_ewma_ug,
                                      self.config.motion_shift)
        self.fast_ug += divpow(raw_mass_ug - self.fast_ug,
                               self.config.fast_shift)
        innovation = raw_mass_ug - precision_mass_ug
        moving = abs(innovation) > self.config.innovation_threshold_ug or \
                 abs(self.motion_ewma_ug) > self.config.motion_threshold_ug
        if gap:
            self._enter(SETTLING, REASON_GAP)
            self.quiet = 0
        elif process_active:
            self._enter(FAST, REASON_PROCESS_ACTIVE)
            self.quiet = 0
        elif moving:
            self._enter(FAST, REASON_MOTION)
            self.quiet = 0
        elif self.state == FAST:
            self.quiet += 1
            if self.quiet >= self.config.quiet_samples:
                self._enter(SETTLING, REASON_QUIET)
                self.quiet = 0
        elif self.state == SETTLING:
            if abs(self.output_ug - precision_mass_ug) <= \
                    self.config.settle_threshold_ug:
                self.quiet += 1
                if self.quiet >= self.config.quiet_samples:
                    self._enter(PRECISION, REASON_CONVERGED)
                    self.output_ug = precision_mass_ug
                    self.quiet = 0
            else:
                self.quiet = 0
        else:
            self.reason = REASON_NONE
        if self.state == FAST:
            self.output_ug += divpow(self.fast_ug - self.output_ug,
                                     self.config.output_shift)
        elif self.state == SETTLING:
            self.output_ug += divpow(precision_mass_ug - self.output_ug,
                                     self.config.output_shift)
        else:
            self.output_ug = precision_mass_ug
        self.dwell = min(0xFFFF, self.dwell + 1)
        self.previous_input_ug = raw_mass_ug
        self.last_timestamp_ms = timestamp_ms
        self.last_sequence = sequence
        return self._result(precision_mass_ug, process_active, gap=gap)

    def _enter(self, state, reason):
        if self.state != state:
            self.state = state
            self.reason = reason
            self.dwell = 0
            self.transition_count = min(0xFFFFFFFF,
                                        self.transition_count + 1)
        elif reason == REASON_PROCESS_ACTIVE:
            self.reason = reason

    def _result(self, precision_mass_ug, process_active, fault=False,
                gap=False):
        stable = self.state == PRECISION and self.dwell >= \
            self.config.stable_samples and not process_active and not fault
        return {
            "raw_mass_ug": int(self.previous_input_ug),
            "precision_mass_ug": int(precision_mass_ug),
            "candidate_mass_ug": int(self.output_ug),
            "innovation_ug": int(self.previous_input_ug - precision_mass_ug),
            "motion_ewma_ug": int(self.motion_ewma_ug),
            "state": self.state,
            "reason": self.reason,
            "dwell": self.dwell,
            "stable_candidate": int(stable),
            "process_active": int(process_active),
            "transition_count": self.transition_count,
            "disturbance_observed": 0,
            "sample_gap_observed": int(gap),
            "fault": int(fault),
        }
