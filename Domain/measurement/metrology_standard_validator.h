#ifndef METROLOGY_STANDARD_VALIDATOR_H
#define METROLOGY_STANDARD_VALIDATOR_H

#include "device_config.h"

typedef enum
{
    METROLOGY_STANDARD_OK = 0,
    METROLOGY_STANDARD_NULL,
    METROLOGY_STANDARD_INVALID_UNIT,
    METROLOGY_STANDARD_INVALID_DIVISION,
    METROLOGY_STANDARD_TOO_MANY_INTERVALS,
    METROLOGY_STANDARD_ZERO_RANGE,
    METROLOGY_STANDARD_OVERFLOW
} MetrologyStandardResult;

MetrologyStandardResult MetrologyStandardValidator_Validate(
    const MetrologyConfig *config);
MassValueUg MetrologyStandardValidator_GetMinimumLoad(
    const MetrologyConfig *config);
MassValueUg MetrologyStandardValidator_GetDisplayOverload(
    const MetrologyConfig *config);

#endif /* METROLOGY_STANDARD_VALIDATOR_H */
