#ifndef LIMIT_CHECKER_H
#define LIMIT_CHECKER_H

#include "device_config.h"
#include "weight_types.h"

#include <stdbool.h>

typedef enum
{
    CHECKWEIGH_DISABLED = 0,
    CHECKWEIGH_LOW,
    CHECKWEIGH_OK,
    CHECKWEIGH_HIGH,
    CHECKWEIGH_OVERLOAD,
    CHECKWEIGH_FAULT
} CheckweighState;

typedef struct
{
    CheckweighState last_stable_state;
    CheckweighState current_output_state;
    bool has_stable_classification;
} LimitChecker;

typedef struct
{
    CheckweighState state;
    MassValueUg evaluated_weight_ug;
    bool qualified;
    bool state_changed;
    bool qualified_ok_transition;
} CheckweighResult;

void LimitChecker_Init(LimitChecker *checker);
void LimitChecker_Reset(LimitChecker *checker);
bool LimitChecker_Process(LimitChecker *checker,
                          const WeightSnapshot *snapshot,
                          const AlarmConfig *config,
                          bool weight_invalid,
                          bool calibration_active,
                          CheckweighResult *result);

#endif /* LIMIT_CHECKER_H */
