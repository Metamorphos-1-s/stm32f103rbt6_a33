"""Temporary Stage 5L-R product-rate containment."""

from hw_common import HardwareTestError


def require_profile_authorization(profile, engineering_40hz=False):
    if profile == 0:
        return
    if profile == 1:
        if not engineering_40hz:
            raise HardwareTestError(
                "Profile 1 / 40 Hz is diagnostic-only pending Stage 5L-R; "
                "use --allow-engineering-40hz with explicit authorization")
        return
    raise HardwareTestError("640/1280 Hz product profiles are prohibited")

