#include "battery_adc.h"

#include "bsp_adc.h"
#include "bsp_time.h"
#include "event_queue.h"
#include "project_config.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

#define BATTERY_MAX_RESISTOR_OHM  1000000U
#define BATTERY_GAIN_SCALE_PPM    1000000LL

static BatteryConfig s_config;
static BatteryAdcState s_state;
static bool s_initialized;
static uint32_t s_error_count;

static bool BatteryAdc_ConfigIsValid(const BatteryConfig *config);

bool BatteryAdc_Init(const BatteryConfig *config)
{
    if (!BatteryAdc_ConfigIsValid(config))
    {
        return false;
    }

    s_config = *config;
    (void)memset(&s_state, 0, sizeof(s_state));
    s_error_count = 0U;
    s_initialized = true;
    return true;
}

void BatteryAdc_Process500ms(void)
{
    AppEvent event;
    uint32_t accumulator = 0U;
    uint16_t raw;
    uint8_t index;

    if (!s_initialized)
    {
        return;
    }

    for (index = 0U; index < BATTERY_ADC_SAMPLE_COUNT; ++index)
    {
        if (!BSP_BatteryAdcReadRaw(&raw))
        {
            s_state.valid = false;
            ++s_error_count;
            return;
        }
        accumulator += raw;
    }

    s_state.raw_average = (uint16_t)((accumulator +
        (BATTERY_ADC_SAMPLE_COUNT / 2U)) / BATTERY_ADC_SAMPLE_COUNT);
    s_state.adc_mv = BSP_BatteryRawToAdcMv(s_state.raw_average,
                                           BATTERY_DEFAULT_VDDA_MV);
    s_state.battery_mv = BatteryAdc_ConvertAdcMv(s_state.adc_mv, &s_config);
    s_state.timestamp_ms = BSP_TimeNowMs();
    s_state.near_full_scale =
        s_state.raw_average >= BATTERY_ADC_NEAR_FULL_SCALE_RAW;
    s_state.valid = true;

    event.type = EVENT_BATTERY_SAMPLE_UPDATED;
    event.timestamp_ms = s_state.timestamp_ms;
    event.arg0 = s_state.battery_mv;
    event.arg1 = s_state.raw_average;
    event.source = NULL;
    if (!EventQueue_Push(&event))
    {
        ++s_error_count;
    }
}

const BatteryAdcState *BatteryAdc_GetState(void)
{
    return s_initialized ? &s_state : NULL;
}

uint32_t BatteryAdc_GetErrorCount(void)
{
    return s_error_count;
}

uint32_t BatteryAdc_ConvertAdcMv(uint32_t adc_mv,
                                 const BatteryConfig *config)
{
    uint64_t total_ohm;
    uint64_t uncalibrated_mv;
    int64_t corrected_mv;

    if (!BatteryAdc_ConfigIsValid(config))
    {
        return 0U;
    }

    total_ohm = (uint64_t)config->divider_top_ohm +
                config->divider_bottom_ohm;
    uncalibrated_mv = ((uint64_t)adc_mv * total_ohm +
        (config->divider_bottom_ohm / 2U)) / config->divider_bottom_ohm;
    if (uncalibrated_mv > (uint64_t)INT32_MAX)
    {
        uncalibrated_mv = (uint64_t)INT32_MAX;
    }

    corrected_mv = (int64_t)uncalibrated_mv;
    corrected_mv += (corrected_mv * config->calibration_gain_ppm) /
                    BATTERY_GAIN_SCALE_PPM;
    corrected_mv += config->calibration_offset_mv;

    if (corrected_mv <= 0)
    {
        return 0U;
    }
    if (corrected_mv > (int64_t)UINT32_MAX)
    {
        return UINT32_MAX;
    }
    return (uint32_t)corrected_mv;
}

static bool BatteryAdc_ConfigIsValid(const BatteryConfig *config)
{
    return (config != NULL) && (config->divider_bottom_ohm != 0U) &&
           (config->divider_top_ohm <= BATTERY_MAX_RESISTOR_OHM) &&
           (config->divider_bottom_ohm <= BATTERY_MAX_RESISTOR_OHM);
}
