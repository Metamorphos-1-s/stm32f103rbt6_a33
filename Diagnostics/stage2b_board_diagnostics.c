#include "stage2b_board_diagnostics.h"

#include "project_config.h"
#include "stage2b_display_font.h"

#include <stddef.h>
#include <string.h>

#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
#include "battery_adc.h"
#include "bsp_time.h"
#include "cs1237.h"
#include "raw_measurement.h"
#include "tm1628.h"
#include "tm1628_board_map.h"
#include "w02_pwrkey.h"

#define STAGE2B_LAMP_MIN_MS       50U
#define STAGE2B_LAMP_MAX_MS       2000U
#define STAGE2B_BUZZER_MIN_MS     20U
#define STAGE2B_BUZZER_MAX_MS     300U

static bool s_active;
static uint32_t s_state_enter_ms;
static uint32_t s_last_display_ms;
static uint32_t s_output_start_ms;
static uint32_t s_output_duration_ms;
static uint32_t s_key_display_start_ms;
static uint8_t s_step_index;
static uint8_t s_digit_phase;
#endif

static Stage2BDiagnosticSnapshot s_snapshot;

#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
static void Stage2B_DiagnosticsEnterState(Stage2BDiagnosticState state);
static bool Stage2B_DiagnosticsRenderSingle(uint8_t digit,
                                            uint16_t logical_segments);
static void Stage2B_DiagnosticsRenderRaw(void);
static void Stage2B_DiagnosticsRenderBattery(void);
static void Stage2B_DiagnosticsRenderKey(void);
static void Stage2B_DiagnosticsRenderConfig(void);
static void Stage2B_DiagnosticsUpdateSnapshot(void);
static bool Stage2B_DiagnosticsOutputDurationValid(OutputId output,
                                                   uint32_t duration_ms);
#endif

void Stage2B_DiagnosticsInit(void)
{
    (void)memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.state = STAGE2B_DIAG_STATE_DISABLED;
    s_snapshot.active_output = OUTPUT_COUNT;

#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    s_active = true;
    s_output_duration_ms = 0U;
    TM1628_Clear();
    Stage2B_DiagnosticsEnterState(STAGE2B_DIAG_STATE_STARTUP);
#if (STAGE2B_DIAG_AUTO_INTERNAL_BEEP != 0U)
    (void)Stage2B_DiagnosticsRequestOutputTest(OUTPUT_INTERNAL_BUZZER, 50U);
#endif
#endif
}

void Stage2B_DiagnosticsProcess(void)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    uint32_t now_ms;

    if (!s_active)
    {
        return;
    }

    now_ms = BSP_TimeNowMs();
    if (s_snapshot.output_test_active &&
        BSP_TimeElapsed(now_ms, s_output_start_ms, s_output_duration_ms))
    {
        (void)OutputGpio_Set(s_snapshot.active_output, false);
        s_snapshot.output_test_active = false;
        s_snapshot.active_output = OUTPUT_COUNT;
    }

    Stage2B_DiagnosticsUpdateSnapshot();

    switch (s_snapshot.state)
    {
        case STAGE2B_DIAG_STATE_STARTUP:
            if (BSP_TimeElapsed(now_ms, s_state_enter_ms,
                                STAGE2B_DIAG_STARTUP_BLANK_MS))
            {
                Stage2B_DiagnosticsEnterState(
                    STAGE2B_DIAG_STATE_SEGMENT_TEST);
            }
            break;
        case STAGE2B_DIAG_STATE_SEGMENT_TEST:
        case STAGE2B_DIAG_STATE_UP_LED_TEST:
        case STAGE2B_DIAG_STATE_DOWN_LED_TEST:
            if (BSP_TimeElapsed(now_ms, s_state_enter_ms,
                                STAGE2B_DIAG_STEP_PERIOD_MS))
            {
                ++s_step_index;
                if (s_step_index < BOARD_DISPLAY_DIGIT_COUNT)
                {
                    uint16_t segments = (s_snapshot.state ==
                        STAGE2B_DIAG_STATE_SEGMENT_TEST) ? 0x00FFU :
                        ((s_snapshot.state == STAGE2B_DIAG_STATE_UP_LED_TEST) ?
                         (uint16_t)(1U << BOARD_SEG_UP) :
                         (uint16_t)(1U << BOARD_SEG_DOWN));
                    s_state_enter_ms = now_ms;
                    (void)Stage2B_DiagnosticsRenderSingle(s_step_index,
                                                          segments);
                }
                else if (s_snapshot.state ==
                         STAGE2B_DIAG_STATE_SEGMENT_TEST)
                {
                    Stage2B_DiagnosticsEnterState(
                        STAGE2B_DIAG_STATE_UP_LED_TEST);
                }
                else if (s_snapshot.state ==
                         STAGE2B_DIAG_STATE_UP_LED_TEST)
                {
                    Stage2B_DiagnosticsEnterState(
                        STAGE2B_DIAG_STATE_DOWN_LED_TEST);
                }
                else
                {
                    Stage2B_DiagnosticsEnterState(
                        STAGE2B_DIAG_STATE_DIGIT_TEST);
                }
            }
            break;
        case STAGE2B_DIAG_STATE_DIGIT_TEST:
            if (BSP_TimeElapsed(now_ms, s_state_enter_ms,
                                STAGE2B_DIAG_PAGE_PERIOD_MS))
            {
                s_state_enter_ms = now_ms;
                if (s_digit_phase == 0U)
                {
                    s_digit_phase = 1U;
                    Stage2B_DiagnosticsRenderConfig();
                }
                else
                {
                    Stage2B_DiagnosticsEnterState(
                        STAGE2B_DIAG_STATE_LIVE_RAW);
                }
            }
            break;
        case STAGE2B_DIAG_STATE_LIVE_RAW:
            if (BSP_TimeElapsed(now_ms, s_last_display_ms,
                                RAW_MEASUREMENT_EVENT_PERIOD_MS))
            {
                s_last_display_ms = now_ms;
                Stage2B_DiagnosticsRenderRaw();
            }
            break;
        case STAGE2B_DIAG_STATE_LIVE_BATTERY:
            if (BSP_TimeElapsed(now_ms, s_last_display_ms, 500U))
            {
                s_last_display_ms = now_ms;
                Stage2B_DiagnosticsRenderBattery();
            }
            break;
        case STAGE2B_DIAG_STATE_KEY_DISPLAY:
            if (BSP_TimeElapsed(now_ms, s_key_display_start_ms,
                                STAGE2B_DIAG_KEY_HOLD_MS))
            {
                Stage2B_DiagnosticsEnterState(
                    STAGE2B_DIAG_STATE_LIVE_RAW);
            }
            break;
        case STAGE2B_DIAG_STATE_DISABLED:
        case STAGE2B_DIAG_STATE_ERROR:
        default:
            break;
    }
#endif
}

bool Stage2B_DiagnosticsIsActive(void)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    return s_active;
#else
    return false;
#endif
}

Stage2BDiagnosticState Stage2B_DiagnosticsGetState(void)
{
    return s_snapshot.state;
}

bool Stage2B_DiagnosticsRequestLampTest(OutputId output,
                                        uint32_t duration_ms)
{
    if ((output < OUTPUT_GREEN_LAMP) || (output > OUTPUT_YELLOW_LAMP))
    {
        return false;
    }
    return Stage2B_DiagnosticsRequestOutputTest(output, duration_ms);
}

bool Stage2B_DiagnosticsRequestOutputTest(OutputId output,
                                          uint32_t duration_ms)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    if (!s_active || s_snapshot.output_test_active ||
        !Stage2B_DiagnosticsOutputDurationValid(output, duration_ms) ||
        !OutputGpio_Set(output, true))
    {
        return false;
    }

    s_output_start_ms = BSP_TimeNowMs();
    s_output_duration_ms = duration_ms;
    s_snapshot.output_test_active = true;
    s_snapshot.active_output = output;
    return true;
#else
    (void)output;
    (void)duration_ms;
    return false;
#endif
}

bool Stage2B_DiagnosticsRequestW02Pulse(uint32_t duration_ms)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    return s_active && W02PwrKey_RequestPulse(duration_ms);
#else
    (void)duration_ms;
    return false;
#endif
}

void Stage2B_DiagnosticsStopAllOutputs(void)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    OutputGpio_AllOff();
    W02PwrKey_Init();
    s_snapshot.output_test_active = false;
    s_snapshot.active_output = OUTPUT_COUNT;
    s_snapshot.w02_pulse_busy = false;
#endif
}

void Stage2B_DiagnosticsStop(void)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    Stage2B_DiagnosticsStopAllOutputs();
    TM1628_Clear();
    (void)TM1628_Flush();
    s_active = false;
    s_snapshot.state = STAGE2B_DIAG_STATE_DISABLED;
#endif
}

void Stage2B_DiagnosticsEnterFault(void)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    Stage2B_DiagnosticsStopAllOutputs();
    TM1628_Clear();
    (void)TM1628_Flush();
    s_active = false;
    s_snapshot.state = STAGE2B_DIAG_STATE_ERROR;
#endif
}

bool Stage2B_DiagnosticsSelectState(Stage2BDiagnosticState state)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    if (!s_active || ((state != STAGE2B_DIAG_STATE_LIVE_RAW) &&
                      (state != STAGE2B_DIAG_STATE_LIVE_BATTERY)))
    {
        return false;
    }
    Stage2B_DiagnosticsEnterState(state);
    return true;
#else
    (void)state;
    return false;
#endif
}

void Stage2B_DiagnosticsSetRawKeyMask(uint8_t key_mask)
{
    s_snapshot.raw_key_mask = key_mask & 0x1FU;
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    if (s_active && ((s_snapshot.state == STAGE2B_DIAG_STATE_LIVE_RAW) ||
                     (s_snapshot.state == STAGE2B_DIAG_STATE_LIVE_BATTERY) ||
                     (s_snapshot.state == STAGE2B_DIAG_STATE_KEY_DISPLAY)))
    {
        s_key_display_start_ms = BSP_TimeNowMs();
        s_snapshot.state = STAGE2B_DIAG_STATE_KEY_DISPLAY;
        Stage2B_DiagnosticsRenderKey();
    }
#endif
}

void Stage2B_DiagnosticsUpdateCs1237Stats(uint16_t backlog,
                                         uint32_t overrun_count)
{
    s_snapshot.cs1237_backlog = backlog;
    s_snapshot.cs1237_overrun_count = overrun_count;
}

const Stage2BDiagnosticSnapshot *Stage2B_DiagnosticsGetSnapshot(void)
{
    return &s_snapshot;
}

bool Stage2B_DisplayText6(const char text[6], uint8_t decimal_point_mask)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    uint16_t logical_masks[BOARD_DISPLAY_DIGIT_COUNT];
    uint8_t index;

    if (text == NULL)
    {
        return false;
    }
    for (index = 0U; index < BOARD_DISPLAY_DIGIT_COUNT; ++index)
    {
        if (!Stage2B_FontEncode(text[index], &logical_masks[index]))
        {
            return false;
        }
        if ((decimal_point_mask & (uint8_t)(1U << index)) != 0U)
        {
            logical_masks[index] |= (uint16_t)(1U << BOARD_SEG_DP);
        }
    }
    for (index = 0U; index < BOARD_DISPLAY_DIGIT_COUNT; ++index)
    {
        if (!TM1628_SetGridSegments(g_board_digit_to_grid[index],
                TM1628_BoardMapSegments(logical_masks[index])))
        {
            return false;
        }
    }
    return true;
#else
    (void)text;
    (void)decimal_point_mask;
    return false;
#endif
}

bool Stage2B_DisplayHex24(uint32_t raw24)
{
    char text[6];

    return Stage2B_FormatHex24(raw24, text) &&
           Stage2B_DisplayText6(text, 0U);
}

bool Stage2B_DisplayBatteryMv(uint32_t battery_mv)
{
    char text[6];
    uint8_t decimal_point_mask;

    return Stage2B_FormatBatteryMv(battery_mv, true, text,
                                   &decimal_point_mask) &&
           Stage2B_DisplayText6(text, decimal_point_mask);
}

#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
static void Stage2B_DiagnosticsEnterState(Stage2BDiagnosticState state)
{
    static const char digits[6] = {'1', '2', '3', '4', '5', '6'};

    s_snapshot.state = state;
    s_state_enter_ms = BSP_TimeNowMs();
    s_last_display_ms = s_state_enter_ms;
    s_step_index = 0U;

    switch (state)
    {
        case STAGE2B_DIAG_STATE_STARTUP:
            TM1628_Clear();
            break;
        case STAGE2B_DIAG_STATE_SEGMENT_TEST:
            (void)Stage2B_DiagnosticsRenderSingle(0U, 0x00FFU);
            break;
        case STAGE2B_DIAG_STATE_UP_LED_TEST:
            (void)Stage2B_DiagnosticsRenderSingle(
                0U, (uint16_t)(1U << BOARD_SEG_UP));
            break;
        case STAGE2B_DIAG_STATE_DOWN_LED_TEST:
            (void)Stage2B_DiagnosticsRenderSingle(
                0U, (uint16_t)(1U << BOARD_SEG_DOWN));
            break;
        case STAGE2B_DIAG_STATE_DIGIT_TEST:
            s_digit_phase = 0U;
            (void)Stage2B_DisplayText6(digits, 0U);
            break;
        case STAGE2B_DIAG_STATE_LIVE_RAW:
            Stage2B_DiagnosticsRenderRaw();
            break;
        case STAGE2B_DIAG_STATE_LIVE_BATTERY:
            Stage2B_DiagnosticsRenderBattery();
            break;
        case STAGE2B_DIAG_STATE_KEY_DISPLAY:
            Stage2B_DiagnosticsRenderKey();
            break;
        case STAGE2B_DIAG_STATE_DISABLED:
        case STAGE2B_DIAG_STATE_ERROR:
        default:
            break;
    }
}

static bool Stage2B_DiagnosticsRenderSingle(uint8_t digit,
                                            uint16_t logical_segments)
{
    uint8_t index;

    if (digit >= BOARD_DISPLAY_DIGIT_COUNT)
    {
        return false;
    }
    for (index = 0U; index < BOARD_DISPLAY_DIGIT_COUNT; ++index)
    {
        uint16_t segments = (index == digit) ? logical_segments : 0U;
        if (!TM1628_SetGridSegments(g_board_digit_to_grid[index],
                TM1628_BoardMapSegments(segments)))
        {
            return false;
        }
    }
    return true;
}

static void Stage2B_DiagnosticsRenderRaw(void)
{
    const RawMeasurementState *raw = RawMeasurement_GetState();
    static const char no_data[6] = {'-', '-', '-', '-', '-', '-'};

    if ((CS1237_GetState() != CS1237_STATE_RUNNING) || !raw->has_sample)
    {
        (void)Stage2B_DisplayText6(no_data, 0U);
    }
    else
    {
        (void)Stage2B_DisplayHex24((uint32_t)raw->latest.raw_value &
                                   0x00FFFFFFUL);
    }
}

static void Stage2B_DiagnosticsRenderBattery(void)
{
    const BatteryAdcState *battery = BatteryAdc_GetState();
    char text[6];
    uint8_t decimal_point_mask;

    if (Stage2B_FormatBatteryMv(
            (battery != NULL) ? battery->battery_mv : 0U,
            (battery != NULL) && battery->valid,
            text, &decimal_point_mask))
    {
        (void)Stage2B_DisplayText6(text, decimal_point_mask);
    }
}

static void Stage2B_DiagnosticsRenderKey(void)
{
    char text[6];

    if (Stage2B_FormatKeyMask(s_snapshot.raw_key_mask, text))
    {
        (void)Stage2B_DisplayText6(text, 0U);
    }
}

static void Stage2B_DiagnosticsRenderConfig(void)
{
    char text[6] = {'C', '-', '0', '0', '0', '0'};
    char hex[6];

    if (Stage2B_FormatHex24(CS1237_GetLastConfigRegister(), hex))
    {
        text[4] = hex[4];
        text[5] = hex[5];
    }
    (void)Stage2B_DisplayText6(text, 0U);
}

static void Stage2B_DiagnosticsUpdateSnapshot(void)
{
    const RawMeasurementState *raw = RawMeasurement_GetState();
    const BatteryAdcState *battery = BatteryAdc_GetState();

    if (raw->has_sample)
    {
        s_snapshot.latest_raw = raw->latest.raw_value;
    }
    s_snapshot.raw_sample_count = raw->accepted_sample_count;
    if (battery != NULL)
    {
        s_snapshot.battery_raw = battery->raw_average;
        s_snapshot.battery_mv = battery->battery_mv;
    }
    s_snapshot.cs1237_config_register = CS1237_GetLastConfigRegister();
    s_snapshot.tm1628_error_count = TM1628_GetErrorCount();
    s_snapshot.w02_pulse_busy = W02PwrKey_IsBusy();
}

static bool Stage2B_DiagnosticsOutputDurationValid(OutputId output,
                                                   uint32_t duration_ms)
{
    if ((uint32_t)output >= (uint32_t)OUTPUT_COUNT)
    {
        return false;
    }
    if ((output == OUTPUT_INTERNAL_BUZZER) ||
        (output == OUTPUT_EXTERNAL_BUZZER))
    {
        return (duration_ms >= STAGE2B_BUZZER_MIN_MS) &&
               (duration_ms <= STAGE2B_BUZZER_MAX_MS);
    }
    return (duration_ms >= STAGE2B_LAMP_MIN_MS) &&
           (duration_ms <= STAGE2B_LAMP_MAX_MS);
}
#endif
