#include "weighing_profile_manager.h"

#include "config_application.h"
#include "cs1237.h"
#include "fault_manager.h"
#include "system_context.h"

#include <stddef.h>

static WeighingProfileSwitchState s_state;
static WeighingProfileId s_target;
static WeighingProfileId s_previous;
static CommandResult s_last_result;

static CS1237_Config MakeConfig(WeighingProfileId id)
{
    const SystemContext *context = SystemContext_Get();
    const WeighingProfileConfig *profile = &context->config.metrology.profiles[id];
    CS1237_Config result;
    result.rate = (CS1237_DataRate)profile->sample_rate;
    result.gain = (CS1237_Gain)profile->gain;
    result.channel = CS1237_CHANNEL_A;
    result.reference_output_enabled = true;
    return result;
}

void WeighingProfileManager_Init(void)
{
    s_state = PROFILE_SWITCH_IDLE;
    s_target = WEIGHING_PROFILE_HIGH_PRECISION;
    s_previous = WEIGHING_PROFILE_HIGH_PRECISION;
    s_last_result = COMMAND_RESULT_OK;
}

CommandResult WeighingProfileManager_Request(WeighingProfileId profile)
{
    const SystemContext *context = SystemContext_Get();
    if ((context == NULL) || ((uint32_t)profile >= WEIGHING_PROFILE_COUNT))
        return COMMAND_RESULT_INVALID_ARGUMENT;
    if (WeighingProfileManager_IsBusy()) return COMMAND_RESULT_BUSY;
    if (profile == context->config.metrology.active_profile)
        return COMMAND_RESULT_OK;
    s_previous = context->config.metrology.active_profile;
    s_target = profile;
    s_last_result = COMMAND_RESULT_ACCEPTED;
    s_state = PROFILE_SWITCH_REQUESTED;
    return COMMAND_RESULT_ACCEPTED;
}

void WeighingProfileManager_Process(void)
{
    CS1237_Sample discarded;
    CS1237_State driver_state;
    const SystemContext *context;
    DeviceConfig candidate;
    switch (s_state)
    {
        case PROFILE_SWITCH_REQUESTED: s_state = PROFILE_SWITCH_PAUSE; break;
        case PROFILE_SWITCH_PAUSE: s_state = PROFILE_SWITCH_CLEAR_FIFO; break;
        case PROFILE_SWITCH_CLEAR_FIFO:
            if (CS1237_TryPopSample(&discarded)) break;
            s_state = PROFILE_SWITCH_CONFIGURE;
            break;
        case PROFILE_SWITCH_CONFIGURE:
        {
            CS1237_Config config = MakeConfig(s_target);
            s_state = CS1237_WriteConfig(&config) ? PROFILE_SWITCH_VERIFY :
                PROFILE_SWITCH_ROLLBACK;
            break;
        }
        case PROFILE_SWITCH_VERIFY:
            driver_state = CS1237_GetState();
            if (driver_state == CS1237_STATE_SETTLING)
                s_state = PROFILE_SWITCH_SETTLING;
            else if (driver_state == CS1237_STATE_ERROR)
                s_state = PROFILE_SWITCH_ROLLBACK;
            break;
        case PROFILE_SWITCH_SETTLING:
            driver_state = CS1237_GetState();
            if (driver_state == CS1237_STATE_RUNNING)
                s_state = PROFILE_SWITCH_RESET_METROLOGY;
            else if (driver_state == CS1237_STATE_ERROR)
                s_state = PROFILE_SWITCH_ROLLBACK;
            break;
        case PROFILE_SWITCH_RESET_METROLOGY:
            context = SystemContext_Get();
            if (context == NULL) { s_state = PROFILE_SWITCH_ROLLBACK; break; }
            candidate = context->config;
            candidate.metrology.active_profile = s_target;
            candidate.metrology.cs1237_data_rate =
                candidate.metrology.profiles[s_target].sample_rate;
            candidate.metrology.cs1237_gain =
                candidate.metrology.profiles[s_target].gain;
            candidate.metrology.filter_mode =
                candidate.metrology.profiles[s_target].filter_mode;
            candidate.metrology.filter_strength =
                candidate.metrology.profiles[s_target].filter_strength;
            if (ConfigApplication_ApplyFactoryDefaults(&candidate) == CONFIG_APPLY_OK)
            {
                s_last_result = COMMAND_RESULT_OK;
                s_state = PROFILE_SWITCH_COMPLETE;
            }
            else s_state = PROFILE_SWITCH_ROLLBACK;
            break;
        case PROFILE_SWITCH_ROLLBACK:
        {
            CS1237_Config config = MakeConfig(s_previous);
            if (!CS1237_WriteConfig(&config))
            {
                FaultManager_Set(FAULT_CS1237_CONFIG_ERROR);
                s_last_result = COMMAND_RESULT_INTERNAL_ERROR;
                s_state = PROFILE_SWITCH_ERROR;
            }
            else
            {
                s_last_result = COMMAND_RESULT_INTERNAL_ERROR;
                s_state = PROFILE_SWITCH_ERROR;
            }
            break;
        }
        case PROFILE_SWITCH_ERROR:
            driver_state = CS1237_GetState();
            if (driver_state == CS1237_STATE_RUNNING) s_state = PROFILE_SWITCH_IDLE;
            else if (driver_state == CS1237_STATE_ERROR)
                FaultManager_Set(FAULT_CS1237_CONFIG_ERROR);
            break;
        case PROFILE_SWITCH_COMPLETE: s_state = PROFILE_SWITCH_IDLE; break;
        case PROFILE_SWITCH_IDLE:
        default: break;
    }
}

bool WeighingProfileManager_IsBusy(void)
{
    return (s_state != PROFILE_SWITCH_IDLE) &&
           (s_state != PROFILE_SWITCH_COMPLETE);
}

WeighingProfileSwitchState WeighingProfileManager_GetState(void) { return s_state; }
CommandResult WeighingProfileManager_GetLastResult(void) { return s_last_result; }
