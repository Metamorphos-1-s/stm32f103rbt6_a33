#include "limit_checker.h"

#include "alarm_config_validation.h"

#include <stddef.h>

static void LimitChecker_ClearQualification(LimitChecker *checker)
{
    checker->last_stable_state = CHECKWEIGH_DISABLED;
    checker->has_stable_classification = false;
}

static CheckweighState LimitChecker_Classify(const LimitChecker *checker,
    MassValueUg weight, const AlarmConfig *config)
{
    if (checker->has_stable_classification &&
        (checker->last_stable_state == CHECKWEIGH_LOW))
    {
        if (weight > config->upper_limit_ug)
        {
            return CHECKWEIGH_HIGH;
        }
        if (weight >= config->lower_limit_ug + config->hysteresis_ug)
        {
            return CHECKWEIGH_OK;
        }
        return CHECKWEIGH_LOW;
    }
    if (checker->has_stable_classification &&
        (checker->last_stable_state == CHECKWEIGH_HIGH))
    {
        if (weight < config->lower_limit_ug)
        {
            return CHECKWEIGH_LOW;
        }
        if (weight <= config->upper_limit_ug - config->hysteresis_ug)
        {
            return CHECKWEIGH_OK;
        }
        return CHECKWEIGH_HIGH;
    }
    if (weight < config->lower_limit_ug)
    {
        return CHECKWEIGH_LOW;
    }
    if (weight > config->upper_limit_ug)
    {
        return CHECKWEIGH_HIGH;
    }
    return CHECKWEIGH_OK;
}

void LimitChecker_Init(LimitChecker *checker)
{
    LimitChecker_Reset(checker);
}

void LimitChecker_Reset(LimitChecker *checker)
{
    if (checker != NULL)
    {
        checker->last_stable_state = CHECKWEIGH_DISABLED;
        checker->current_output_state = CHECKWEIGH_DISABLED;
        checker->has_stable_classification = false;
    }
}

bool LimitChecker_Process(LimitChecker *checker,
                          const WeightSnapshot *snapshot,
                          const AlarmConfig *config,
                          bool weight_invalid,
                          bool calibration_active,
                          CheckweighResult *result)
{
    CheckweighState next_state;
    CheckweighState previous_stable;
    MassValueUg weight;
    bool stable;

    if ((checker == NULL) || (snapshot == NULL) || (config == NULL) ||
        (result == NULL))
    {
        return false;
    }

    weight = (config->weight_source == ALARM_WEIGHT_GROSS) ?
        snapshot->gross_mass_ug : snapshot->net_mass_ug;
    result->evaluated_weight_ug = weight;
    result->qualified = false;
    result->qualified_ok_transition = false;

    if (calibration_active || !config->limit_function_enable)
    {
        LimitChecker_ClearQualification(checker);
    }

    if (weight_invalid || !AlarmConfig_Validate(config))
    {
        next_state = CHECKWEIGH_FAULT;
    }
    else if ((snapshot->status_flags & WEIGHT_STATUS_OVERLOAD) != 0U)
    {
        next_state = CHECKWEIGH_OVERLOAD;
    }
    else if (calibration_active || !config->limit_function_enable)
    {
        next_state = CHECKWEIGH_DISABLED;
    }
    else
    {
        stable = (snapshot->status_flags & WEIGHT_STATUS_STABLE) != 0U;
        if (!stable)
        {
            next_state = checker->has_stable_classification ?
                checker->last_stable_state : CHECKWEIGH_DISABLED;
        }
        else
        {
            previous_stable = checker->last_stable_state;
            next_state = LimitChecker_Classify(checker, weight, config);
            result->qualified = true;
            result->qualified_ok_transition =
                (next_state == CHECKWEIGH_OK) &&
                (!checker->has_stable_classification ||
                 (previous_stable != CHECKWEIGH_OK));
            checker->last_stable_state = next_state;
            checker->has_stable_classification = true;
        }
    }

    result->state = next_state;
    result->state_changed = next_state != checker->current_output_state;
    checker->current_output_state = next_state;
    return true;
}
