#include "output_gpio.h"

#include "bsp_gpio.h"

#include <stdint.h>

static bool s_output_state[OUTPUT_COUNT];

static bool OutputGpio_IsValid(OutputId output);
static void OutputGpio_Apply(OutputId output, bool enabled);

void OutputGpio_Init(void)
{
    OutputGpio_AllOff();
}

bool OutputGpio_Set(OutputId output, bool enabled)
{
    if (!OutputGpio_IsValid(output))
    {
        return false;
    }
    if (s_output_state[(uint8_t)output] == enabled)
    {
        return true;
    }

    OutputGpio_Apply(output, enabled);
    s_output_state[(uint8_t)output] = enabled;
    return true;
}

bool OutputGpio_Get(OutputId output)
{
    if (!OutputGpio_IsValid(output))
    {
        return false;
    }
    return s_output_state[(uint8_t)output];
}

void OutputGpio_AllOff(void)
{
    uint8_t index;

    for (index = 0U; index < (uint8_t)OUTPUT_COUNT; ++index)
    {
        OutputGpio_Apply((OutputId)index, false);
        s_output_state[index] = false;
    }
}

static bool OutputGpio_IsValid(OutputId output)
{
    return ((uint32_t)output < (uint32_t)OUTPUT_COUNT);
}

static void OutputGpio_Apply(OutputId output, bool enabled)
{
    switch (output)
    {
        case OUTPUT_INTERNAL_BUZZER:
            BSP_InternalBuzzer_Set(enabled);
            break;
        case OUTPUT_EXTERNAL_BUZZER:
            BSP_ExternalBuzzer_Set(enabled);
            break;
        case OUTPUT_GREEN_LAMP:
            BSP_LimitGreen_Set(enabled);
            break;
        case OUTPUT_RED_LAMP:
            BSP_LimitRed_Set(enabled);
            break;
        case OUTPUT_YELLOW_LAMP:
            BSP_LimitYellow_Set(enabled);
            break;
        case OUTPUT_COUNT:
        default:
            break;
    }
}
