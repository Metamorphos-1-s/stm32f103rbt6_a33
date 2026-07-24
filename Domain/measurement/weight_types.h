#ifndef WEIGHT_TYPES_H
#define WEIGHT_TYPES_H

#include "mass_types.h"

#include <stdint.h>

typedef int32_t WeightValue;

typedef enum
{
    WEIGHT_STATUS_NONE              = 0U,
    WEIGHT_STATUS_RAW_VALID         = 1U << 0,
    WEIGHT_STATUS_FILTER_READY      = 1U << 1,
    WEIGHT_STATUS_CALIBRATION_VALID = 1U << 2,
    WEIGHT_STATUS_WEIGHT_VALID      = 1U << 3,
    WEIGHT_STATUS_STABLE            = 1U << 4,
    WEIGHT_STATUS_ZERO              = 1U << 5,
    WEIGHT_STATUS_TARE_ACTIVE       = 1U << 6,
    WEIGHT_STATUS_OVERLOAD          = 1U << 7
} WeightStatusFlag;

typedef enum
{
    WEIGHT_ACTION_OK = 0,
    WEIGHT_ACTION_NO_SAMPLE,
    WEIGHT_ACTION_FILTER_NOT_READY,
    WEIGHT_ACTION_CALIBRATION_INVALID,
    WEIGHT_ACTION_NOT_STABLE,
    WEIGHT_ACTION_OUT_OF_ZERO_RANGE,
    WEIGHT_ACTION_TARE_ACTIVE,
    WEIGHT_ACTION_OVERLOAD,
    WEIGHT_ACTION_INVALID_ARGUMENT,
    WEIGHT_ACTION_INTERNAL_ERROR
} WeightActionResult;

typedef struct
{
    int32_t raw_value;
    int32_t filtered_raw;
    WeightValue gross_unrounded;
    WeightValue gross_weight;
    WeightValue tare_weight;
    WeightValue net_unrounded;
    WeightValue net_weight;
    uint32_t status_flags;
    uint32_t sample_timestamp_ms;
    uint32_t sample_sequence;
    uint32_t filter_sample_count;
    uint32_t stability_spread;
    MassValueUg gross_mass_ug;
    MassValueUg tare_mass_ug;
    MassValueUg net_mass_ug;
    MassValueUg stability_spread_ug;
} MassSnapshot;

typedef MassSnapshot WeightSnapshot;

#endif /* WEIGHT_TYPES_H */
