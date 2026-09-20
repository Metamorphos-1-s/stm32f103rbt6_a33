"""Integer reference model for Stage 5N-A alarm shadow candidates."""

from dataclasses import dataclass


INVALID, PENDING, LOW, OK, HIGH = range(5)
SUPPRESS_NONE = 0
SUPPRESS_CONFIG = 1
SUPPRESS_INPUT = 2
SUPPRESS_FAULT = 3
SUPPRESS_OVERLOAD = 4
SUPPRESS_CALIBRATION = 5
SUPPRESS_PROCESS_ACTIVE = 6
SUPPRESS_UNSTABLE = 7
SUPPRESS_CONFIRMING = 8
SUPPRESS_SEQUENCE = 9
SUPPRESS_TIMESTAMP = 10
SUPPRESS_RESET = 11
RESET_NONE = 0


@dataclass(frozen=True)
class ShadowConfig:
    static_stable_samples: int
    dynamic_hysteresis_ug: int
    dynamic_confirm_samples: int
    dynamic_min_dwell_samples: int


def round_div(value, divisor):
    sign = -1 if value < 0 else 1
    return sign * ((abs(value) + abs(divisor) // 2) // abs(divisor)) * \
        (1 if divisor > 0 else -1)


def calibrate_raw(raw, raw_zero=-44047, raw_span=-487965,
                  span_mass_ug=500000000, zero_offset_raw=0):
    return round_div((raw - raw_zero - zero_offset_raw) * span_mass_ug,
                     raw_span - raw_zero)


def classify(weight, low_limit, high_limit):
    if low_limit > high_limit:
        return INVALID
    if weight < low_limit:
        return LOW
    if weight > high_limit:
        return HIGH
    return OK


class CheckweighShadow:
    def __init__(self, config):
        self.config = config
        self.reset()

    def reset(self):
        self.last_sequence = None
        self.last_timestamp_ms = None
        self.dynamic_last_change_ms = 0
        self.event_count = 0
        self.static_stable_count = 0
        self.static_last_valid = INVALID
        self.dynamic_confirmed = PENDING
        self.dynamic_candidate = PENDING
        self.dynamic_confirm_count = 0

    @staticmethod
    def _suppression(valid, config_valid, fault, overload, calibration,
                     process_active, stable, static_path):
        if not config_valid:
            return SUPPRESS_CONFIG
        if not valid:
            return SUPPRESS_INPUT
        if fault:
            return SUPPRESS_FAULT
        if overload:
            return SUPPRESS_OVERLOAD
        if calibration:
            return SUPPRESS_CALIBRATION
        if static_path and process_active:
            return SUPPRESS_PROCESS_ACTIVE
        if static_path and not stable:
            return SUPPRESS_UNSTABLE
        return SUPPRESS_NONE

    def _hysteresis(self, weight, low_limit, high_limit):
        h = self.config.dynamic_hysteresis_ug
        state = self.dynamic_confirmed
        if state == LOW:
            if weight > high_limit:
                return HIGH
            return OK if weight >= low_limit + h else LOW
        if state == HIGH:
            if weight < low_limit:
                return LOW
            return OK if weight <= high_limit - h else HIGH
        if state == OK:
            if weight < low_limit - h:
                return LOW
            if weight > high_limit + h:
                return HIGH
            return OK
        return classify(weight, low_limit, high_limit)

    def process(self, *, sequence, timestamp_ms, static_weight_ug,
                dynamic_weight_ug, low_limit_ug, high_limit_ug, stable,
                process_active, valid=True, fault=False, overload=False,
                calibration=False, reset_reason=RESET_NONE):
        sequence_error = self.last_sequence is not None and \
            sequence != ((self.last_sequence + 1) & 0xFFFFFFFF)
        timestamp_error = self.last_timestamp_ms is not None and \
            (timestamp_ms <= self.last_timestamp_ms or
             timestamp_ms - self.last_timestamp_ms > 250)
        explicit_reset = reset_reason != RESET_NONE
        if explicit_reset or sequence_error or timestamp_error:
            self.reset()
        self.last_sequence = sequence
        self.last_timestamp_ms = timestamp_ms
        config_valid = low_limit_ug <= high_limit_ug

        static_immediate = classify(static_weight_ug, low_limit_ug,
                                    high_limit_ug)
        static_reason = self._suppression(valid, config_valid, fault,
            overload, calibration, process_active, stable, True)
        if sequence_error:
            static_reason = SUPPRESS_SEQUENCE
        elif timestamp_error:
            static_reason = SUPPRESS_TIMESTAMP
        elif explicit_reset:
            static_reason = SUPPRESS_RESET
        if static_reason != SUPPRESS_NONE:
            self.static_stable_count = 0
            static_output = INVALID if static_reason in (SUPPRESS_CONFIG,
                SUPPRESS_INPUT, SUPPRESS_FAULT, SUPPRESS_OVERLOAD,
                SUPPRESS_CALIBRATION, SUPPRESS_SEQUENCE,
                SUPPRESS_TIMESTAMP) else PENDING
        else:
            self.static_stable_count = min(255, self.static_stable_count + 1)
            if self.static_stable_count >= self.config.static_stable_samples:
                self.static_last_valid = static_immediate
                static_output = static_immediate
            else:
                static_output = PENDING
                static_reason = SUPPRESS_CONFIRMING

        dynamic_immediate = classify(dynamic_weight_ug, low_limit_ug,
                                     high_limit_ug)
        dynamic_reason = self._suppression(valid, config_valid, fault,
            overload, calibration, False, True, False)
        if sequence_error:
            dynamic_reason = SUPPRESS_SEQUENCE
        elif timestamp_error:
            dynamic_reason = SUPPRESS_TIMESTAMP
        elif explicit_reset:
            dynamic_reason = SUPPRESS_RESET
        event = False
        if dynamic_reason != SUPPRESS_NONE:
            self.dynamic_candidate = PENDING
            self.dynamic_confirm_count = 0
            dynamic_output = PENDING if dynamic_reason == SUPPRESS_RESET else INVALID
        else:
            target = self._hysteresis(dynamic_weight_ug, low_limit_ug,
                                      high_limit_ug)
            if target != self.dynamic_candidate:
                self.dynamic_candidate = target
                self.dynamic_confirm_count = 1
            else:
                self.dynamic_confirm_count = min(255,
                    self.dynamic_confirm_count + 1)
            dwell_ms = self.config.dynamic_min_dwell_samples * 100
            if self.dynamic_confirm_count >= self.config.dynamic_confirm_samples and \
                    (self.dynamic_confirmed == PENDING or
                     timestamp_ms - self.dynamic_last_change_ms >= dwell_ms):
                if target != self.dynamic_confirmed:
                    had_valid_class = self.dynamic_confirmed in (LOW, OK, HIGH)
                    self.dynamic_confirmed = target
                    self.dynamic_last_change_ms = timestamp_ms
                    if had_valid_class:
                        self.event_count += 1
                        event = True
            dynamic_output = self.dynamic_confirmed
            if dynamic_output == PENDING:
                dynamic_reason = SUPPRESS_CONFIRMING
        return {"static_input_ug": static_weight_ug,
            "dynamic_input_ug": dynamic_weight_ug, "stable": int(stable),
            "process_active": int(process_active), "valid": int(valid),
            "static_immediate": static_immediate,
            "static_class": static_output,
            "static_last_valid": self.static_last_valid,
            "static_stable_count": self.static_stable_count,
            "static_reason": static_reason,
            "dynamic_immediate": dynamic_immediate,
            "dynamic_candidate": self.dynamic_candidate,
            "dynamic_confirmed": dynamic_output,
            "dynamic_confirm_count": self.dynamic_confirm_count,
            "dynamic_reason": dynamic_reason,
            "reset_reason": reset_reason if explicit_reset else
                (SUPPRESS_SEQUENCE if sequence_error else
                 SUPPRESS_TIMESTAMP if timestamp_error else RESET_NONE),
            "event": int(event), "event_count": self.event_count}
