#include "display_controller.h"

#include "battery_adc.h"
#include "bsp_time.h"
#include "display_model.h"
#include "display_formatter.h"
#include "metrology_manager.h"
#include "project_config.h"
#include "system_context.h"
#include "tm1628.h"
#include "tm1628_board_map.h"
#include "unit_converter.h"
#if (A33_ENABLE_STAGE5P_R5_CHECKWEIGH_INTEGRATION != 0U)
#include "app_main.h"
#endif
#if (A33_ENABLE_STAGE5MR5_BETA != 0U) && \
    (A33_ENABLE_STAGE5MR5E_D1D_BETA == 0U)
#include "directional_display_follower.h"
#endif

#include <stddef.h>
#include <string.h>

#define SEGMENT_BIT(segment) ((uint16_t)(1U << (uint8_t)(segment)))

static DisplayPage s_page;
static uint32_t s_message_start_ms;
static uint32_t s_message_duration_ms;
static uint32_t s_applied_revision;
static char s_text[6];
static char s_message_text[6];
static uint16_t s_numeric_segments[6];
static bool s_text_override;
static bool s_numeric_override;
static bool s_message_active;
static bool s_initialized;
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
static uint32_t s_last_activity_ms;
#define DISPLAY_IDLE_DIM_MS 600000U
#if (A33_ENABLE_STAGE5P_PANEL_BATTERY_ZERO_LONGRATE != 0U)
#define DISPLAY_LOW_BATTERY_REPEAT_MS 20000U
#define DISPLAY_LOW_BATTERY_WINDOW_MS 4000U
#else
#define DISPLAY_LOW_BATTERY_REPEAT_MS 60000U
#define DISPLAY_LOW_BATTERY_WINDOW_MS 2000U
#endif
#endif
#if (A33_ENABLE_STAGE5MR5_BETA != 0U) && \
    (A33_ENABLE_STAGE5MR5E_D1D_BETA == 0U)
static DirectionalDisplayFollower s_directional_follower;

static uint8_t DisplayController_DivisionCode(uint8_t division)
{
    if (division == 1U) return 0U;
    if (division == 2U) return 1U;
    if (division == 5U) return 2U;
    return 3U;
}

static uint8_t DisplayController_DirectionalSource(DisplayPage page,
    MassUnit unit, const UnitDisplayConfig *display)
{
    return (uint8_t)(((page == DISPLAY_PAGE_GROSS) ? 0x80U : 0U) |
        (((uint8_t)unit & 0x03U) << 5U) |
        ((display->decimal_places & 0x07U) << 2U) |
        DisplayController_DivisionCode(display->division_digit));
}
#endif

static bool DisplayController_ApplyEditCursor(uint16_t segments[6],
    uint8_t selected_digit, bool cursor_visible)
{
    uint8_t display_index;
    uint16_t decimal_point;

    if ((segments == NULL) || (selected_digit >= 6U)) return false;
    display_index = (uint8_t)(5U - selected_digit);
    decimal_point = (uint16_t)(segments[display_index] &
        SEGMENT_BIT(BOARD_SEG_DP));
    if (!cursor_visible)
    {
        segments[display_index] = decimal_point;
    }
    else if ((segments[display_index] &
              (uint16_t)~SEGMENT_BIT(BOARD_SEG_DP)) == 0U)
    {
        if (!DisplayFormatter_EncodeCharacter('0',
                &segments[display_index])) return false;
        segments[display_index] |= decimal_point;
    }
    return true;
}

static void DisplayController_BuildIndicators(const WeightSnapshot *snapshot,
    uint8_t *top, uint8_t *bottom)
{
    uint32_t flags = (snapshot != NULL) ? snapshot->status_flags : 0U;

    *top = 0U;
    *bottom = 0U;
    if (s_page == DISPLAY_PAGE_NET) *top |= DISPLAY_TOP_LED_NET;
    if (s_page == DISPLAY_PAGE_GROSS) *top |= DISPLAY_TOP_LED_GROSS;
#if (A33_ENABLE_STAGE5P_PANEL_BATTERY_ZERO_LONGRATE == 0U)
    /* The unified UI no longer exposes the tare-amount page. */
    if (s_page == DISPLAY_PAGE_TARE) *top |= DISPLAY_TOP_LED_TARE;
#endif
#if (A33_ENABLE_STAGE5P_R5_CHECKWEIGH_INTEGRATION != 0U)
    if ((flags & WEIGHT_STATUS_TARE_ACTIVE) != 0U)
        *top |= DISPLAY_TOP_LED_TARE;
#endif
    if (s_page == DISPLAY_PAGE_BATTERY) *top |= DISPLAY_TOP_LED_BATTERY;
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
    {
        const BatteryAdcState *battery = BatteryAdc_GetState();
        if (battery != NULL && battery->valid && battery->low_voltage_warning) {
            *top &= (uint8_t)(0xFFU ^ DISPLAY_TOP_LED_BATTERY);
            if (((BSP_TimeNowMs() / 500U) & 1U) == 0U)
                *top |= DISPLAY_TOP_LED_BATTERY;
        }
    }
#endif
    if ((flags & WEIGHT_STATUS_STABLE) != 0U) *top |= DISPLAY_TOP_LED_STABLE;
    if ((flags & WEIGHT_STATUS_ZERO) != 0U) *top |= DISPLAY_TOP_LED_ZERO;
#if (A33_ENABLE_STAGE5P_R5_CHECKWEIGH_INTEGRATION == 0U)
    if ((flags & WEIGHT_STATUS_CALIBRATION_VALID) == 0U)
        *bottom |= DISPLAY_BOTTOM_LED_NOCAL;
    if ((flags & WEIGHT_STATUS_TARE_ACTIVE) != 0U)
        *bottom |= DISPLAY_BOTTOM_LED_TARE;
    if ((flags & WEIGHT_STATUS_OVERLOAD) != 0U)
        *bottom |= DISPLAY_BOTTOM_LED_OVERLOAD;
    if ((s_page == DISPLAY_PAGE_MENU) || (s_page == DISPLAY_PAGE_EDIT) ||
        (s_page == DISPLAY_PAGE_CALIBRATION))
        *bottom |= DISPLAY_BOTTOM_LED_MENU;
#else
    {
        AlarmOutputDiagnostics alarm = {0};
        const SystemContext *context = SystemContext_Get();
        if (App_GetAlarmOutputDiagnostics(&alarm))
        {
            if (alarm.checkweigh_state == CHECKWEIGH_LOW)
                *bottom |= DISPLAY_BOTTOM_LED_LOW;
            else if (alarm.checkweigh_state == CHECKWEIGH_OK)
                *bottom |= DISPLAY_BOTTOM_LED_OK;
            else if (alarm.checkweigh_state == CHECKWEIGH_HIGH)
                *bottom |= DISPLAY_BOTTOM_LED_HIGH;
        }
        if (context != NULL && context->config.metrology.active_unit == MASS_UNIT_LB)
            *bottom |= DISPLAY_BOTTOM_LED_LB;
        else
            *bottom |= DISPLAY_BOTTOM_LED_G_KG;
    }
#endif
}

static bool DisplayController_BuildModel(void)
{
    const SystemContext *context = SystemContext_Get();
    const WeightSnapshot *snapshot = MetrologyManager_GetSnapshot();
    const DisplayConditionSnapshot *conditioned =
        MetrologyManager_GetDisplayConditionSnapshot();
    const BatteryAdcState *battery = BatteryAdc_GetState();
    uint8_t top;
    uint8_t bottom;
    uint32_t flags = (snapshot != NULL) ? snapshot->status_flags : 0U;
    DisplayWeightValue converted;
    MassValueUg mass = 0;
    const UnitDisplayConfig *unit_display;

    (void)DisplayModel_SetPage(s_page);
    DisplayController_BuildIndicators(snapshot, &top, &bottom);
    if (!DisplayModel_SetIndicators(top, bottom))
    {
        return false;
    }
    if (s_message_active && (s_page != DISPLAY_PAGE_FAULT))
    {
        return DisplayModel_SetText6(s_message_text);
    }
    if (s_text_override)
    {
        return DisplayModel_SetText6(s_text);
    }
    if (s_numeric_override)
    {
        return DisplayModel_SetRawSegments(s_numeric_segments);
    }
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
    if ((s_page == DISPLAY_PAGE_NET || s_page == DISPLAY_PAGE_GROSS) &&
        battery != NULL && battery->valid && battery->low_voltage_warning &&
        (BSP_TimeNowMs() % DISPLAY_LOW_BATTERY_REPEAT_MS) <
            DISPLAY_LOW_BATTERY_WINDOW_MS &&
        ((BSP_TimeNowMs() / 500U) & 1U) == 0U)
        return DisplayModel_SetText6("Lo-bAt");
#endif
    switch (s_page)
    {
        case DISPLAY_PAGE_NET:
            mass = ((context != NULL) &&
                    (context->runtime.weight_view == WEIGHT_VIEW_NET) &&
                    (conditioned != NULL)) ? conditioned->display_mass_ug :
                   ((snapshot != NULL) ? snapshot->net_mass_ug : 0);
            break;
        case DISPLAY_PAGE_GROSS:
            mass = ((context != NULL) &&
                    (context->runtime.weight_view == WEIGHT_VIEW_GROSS) &&
                    (conditioned != NULL)) ? conditioned->display_mass_ug :
                   ((snapshot != NULL) ? snapshot->gross_mass_ug : 0);
            break;
        case DISPLAY_PAGE_TARE:
            mass = (snapshot != NULL) ? snapshot->tare_mass_ug : 0;
            break;
        case DISPLAY_PAGE_BATTERY:
            return (battery != NULL) && battery->valid ?
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
                DisplayModel_SetBatteryPercent(
                    BatteryAdc_ApproxPercent(battery->battery_mv)) :
#else
                DisplayModel_SetBatteryMv(battery->battery_mv) :
#endif
                DisplayModel_SetText6("------");
        case DISPLAY_PAGE_STATUS:
            return DisplayModel_SetText6(" StAt ");
        case DISPLAY_PAGE_FAULT:
            return DisplayModel_SetText6(" Err  ");
        case DISPLAY_PAGE_BOOT:
        case DISPLAY_PAGE_MENU:
        case DISPLAY_PAGE_EDIT:
        case DISPLAY_PAGE_CALIBRATION:
        case DISPLAY_PAGE_MESSAGE:
        default:
            return true;
    }
    if (context == NULL) return false;
    unit_display = &context->config.metrology.unit_display[
        context->config.metrology.active_unit];
    if (!UnitConverter_MassToDisplay(mass,
            context->config.metrology.active_unit, unit_display, &converted))
        return DisplayModel_SetText6("  Lo  ");
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    if ((((s_page == DISPLAY_PAGE_NET) &&
          (context->runtime.weight_view == WEIGHT_VIEW_NET)) ||
         ((s_page == DISPLAY_PAGE_GROSS) &&
          (context->runtime.weight_view == WEIGHT_VIEW_GROSS))) &&
        (conditioned != NULL) &&
        conditioned->display_domain_valid)
    {
        converted.display_count = conditioned->display_count;
    }
#elif (A33_ENABLE_STAGE5MR5_BETA != 0U)
    if ((s_page == DISPLAY_PAGE_NET) || (s_page == DISPLAY_PAGE_GROSS))
    {
        DirectionalDisplayFollowerInput input;
        DirectionalDisplayFollowerOutput output;
        DisplayWeightValue desired;
        MassValueUg authoritative = (s_page == DISPLAY_PAGE_NET) ?
            ((snapshot != NULL) ? snapshot->net_mass_ug : 0) :
            ((snapshot != NULL) ? snapshot->gross_mass_ug : 0);
        uint8_t division = unit_display->division_digit;
        bool desired_valid = UnitConverter_MassToDisplay(authoritative,
            context->config.metrology.active_unit, unit_display, &desired) &&
            desired.valid && !desired.overflow && (division != 0U) &&
            ((desired.display_count % division) == 0) &&
            ((converted.display_count % division) == 0);
        input.desired_count = desired_valid ?
            desired.display_count / division : converted.display_count;
        input.baseline_count = desired_valid ?
            converted.display_count / division : converted.display_count;
        input.sample_sequence = (snapshot != NULL) ?
            snapshot->sample_sequence : 0U;
        input.source = DisplayController_DirectionalSource(s_page,
            context->config.metrology.active_unit, unit_display);
        input.stable = (flags & WEIGHT_STATUS_STABLE) != 0U;
        input.active = MetrologyManager_GetR5Application() ==
            R5_BETA_APPLICATION_ACTIVE;
        input.valid = converted.valid && !converted.overflow && desired_valid;
        if (DirectionalDisplayFollower_Process(&s_directional_follower,
            &input, &output) && desired_valid)
            converted.display_count = output.display_count * division;
    }
#endif
    return DisplayModel_SetWeight(converted.display_count,
        converted.decimal_places, true, (uint8_t)flags);
}

static bool DisplayController_ApplyModel(void)
{
    const DisplayModel *model = DisplayModel_Get();
    uint8_t index;

    if (model->revision == s_applied_revision)
    {
        return true;
    }
    if (!TM1628_SetBrightness(model->brightness) ||
        !TM1628_SetDisplayEnabled(model->enabled))
    {
        return false;
    }
    for (index = 0U; index < 6U; ++index)
    {
        uint16_t logical = model->digit_segments[index];
        if ((model->top_led_mask & (uint8_t)(1U << index)) != 0U)
            logical |= SEGMENT_BIT(BOARD_SEG_UP);
        if ((model->bottom_led_mask & (uint8_t)(1U << index)) != 0U)
            logical |= SEGMENT_BIT(BOARD_SEG_DOWN);
        if (!TM1628_SetGridSegments(g_board_digit_to_grid[index],
                                    TM1628_BoardMapSegments(logical)))
        {
            return false;
        }
    }
    s_applied_revision = model->revision;
    return true;
}

bool DisplayController_Init(void)
{
    const SystemContext *context = SystemContext_Get();

    DisplayModel_Init();
    s_page = ((context != NULL) &&
              (context->runtime.weight_view == WEIGHT_VIEW_GROSS)) ?
             DISPLAY_PAGE_GROSS : DISPLAY_PAGE_NET;
    s_message_active = false;
    s_text_override = false;
    s_numeric_override = false;
    s_applied_revision = 0U;
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
    s_last_activity_ms = BSP_TimeNowMs();
#endif
    if ((context != NULL) &&
        !DisplayModel_SetBrightness(context->config.display.brightness))
    {
        return false;
    }
    s_initialized = true;
    return true;
}

void DisplayController_Process20ms(void)
{
    AppState state = SystemContext_GetState();
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
    const SystemContext *context = SystemContext_Get();
    const WeightSnapshot *weight = MetrologyManager_GetSnapshot();
    uint32_t now = BSP_TimeNowMs();
#endif

    if (!s_initialized || (state == APP_STATE_DIAGNOSTIC))
    {
        return;
    }
    if (state == APP_STATE_SELF_TEST)
    {
        (void)DisplayController_ApplyModel();
        return;
    }
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
    if (context != NULL && state == APP_STATE_RUN &&
        (s_page == DISPLAY_PAGE_NET || s_page == DISPLAY_PAGE_GROSS ||
         s_page == DISPLAY_PAGE_TARE || s_page == DISPLAY_PAGE_BATTERY)) {
        if (weight == NULL ||
            (weight->status_flags & WEIGHT_STATUS_STABLE) == 0U ||
            s_message_active) s_last_activity_ms = now;
        (void)DisplayModel_SetBrightness(
            (uint32_t)(now - s_last_activity_ms) >= DISPLAY_IDLE_DIM_MS ?
                (context->config.display.brightness > 1U ? 1U :
                    context->config.display.brightness) :
                context->config.display.brightness);
    } else s_last_activity_ms = now;
#endif
    if (s_message_active &&
        ((uint32_t)(BSP_TimeNowMs() - s_message_start_ms) >=
         s_message_duration_ms))
    {
        s_message_active = false;
    }
    if (!DisplayController_BuildModel())
    {
        return;
    }
    (void)DisplayController_ApplyModel();
}

#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
void DisplayController_NotifyActivity(void)
{
    const SystemContext *context = SystemContext_Get();
    s_last_activity_ms = BSP_TimeNowMs();
    if (context != NULL &&
        (s_page == DISPLAY_PAGE_NET || s_page == DISPLAY_PAGE_GROSS ||
         s_page == DISPLAY_PAGE_TARE || s_page == DISPLAY_PAGE_BATTERY))
        (void)DisplayModel_SetBrightness(context->config.display.brightness);
}
#endif

void DisplayController_SetPage(DisplayPage page)
{
    if ((uint32_t)page <= (uint32_t)DISPLAY_PAGE_FAULT)
    {
        s_page = page;
        s_text_override = false;
        s_numeric_override = false;
        s_message_active = false;
    }
}

void DisplayController_ShowMessage(const char text[6], uint32_t duration_ms)
{
    uint16_t segments[6];
    if ((text == NULL) || (duration_ms == 0U))
    {
        return;
    }
    if (!DisplayFormatter_FormatText6(text, segments)) return;
    (void)memcpy(s_message_text, text, 6U);
    s_message_active = true;
    s_message_start_ms = BSP_TimeNowMs();
    s_message_duration_ms = duration_ms;
}

bool DisplayController_SetTextPage(DisplayPage page, const char text[6])
{
    uint16_t segments[6];
    if ((text == NULL) || ((uint32_t)page > (uint32_t)DISPLAY_PAGE_FAULT))
    {
        return false;
    }
    if (!DisplayFormatter_FormatText6(text, segments)) return false;
    (void)memcpy(s_text, text, 6U);
    s_text_override = true;
    s_numeric_override = false;
    s_page = page;
    if (page == DISPLAY_PAGE_FAULT) s_message_active = false;
    return true;
}

bool DisplayController_SetNumericPage(DisplayPage page, int32_t display_count,
                                      uint8_t decimal_places)
{
    uint16_t segments[6];
    if (((uint32_t)page > (uint32_t)DISPLAY_PAGE_FAULT) ||
        !DisplayFormatter_FormatWeight(display_count, decimal_places, true,
                                       segments))
    {
        return false;
    }
    s_text_override = false;
    s_numeric_override = true;
    (void)memcpy(s_numeric_segments, segments, sizeof(s_numeric_segments));
    s_page = page;
    if (page == DISPLAY_PAGE_FAULT) s_message_active = false;
    return true;
}

bool DisplayController_SetTextEditPage(DisplayPage page, const char text[6],
    uint8_t selected_digit, bool cursor_visible)
{
    uint16_t segments[6];
    if ((text == NULL) || ((uint32_t)page > (uint32_t)DISPLAY_PAGE_FAULT) ||
        !DisplayFormatter_FormatText6(text, segments) ||
        !DisplayController_ApplyEditCursor(segments, selected_digit,
                                           cursor_visible))
        return false;
    s_text_override = false;
    s_numeric_override = true;
    (void)memcpy(s_numeric_segments, segments, sizeof(s_numeric_segments));
    s_page = page;
    if (page == DISPLAY_PAGE_FAULT) s_message_active = false;
    return true;
}

bool DisplayController_SetNumericEditPage(DisplayPage page,
    int32_t display_count, uint8_t decimal_places, uint8_t selected_digit,
    bool cursor_visible)
{
    uint16_t segments[6];
    if (((uint32_t)page > (uint32_t)DISPLAY_PAGE_FAULT) ||
        !DisplayFormatter_FormatWeight(display_count, decimal_places, true,
                                       segments) ||
        !DisplayController_ApplyEditCursor(segments, selected_digit,
                                           cursor_visible))
        return false;
    s_text_override = false;
    s_numeric_override = true;
    (void)memcpy(s_numeric_segments, segments, sizeof(s_numeric_segments));
    s_page = page;
    if (page == DISPLAY_PAGE_FAULT) s_message_active = false;
    return true;
}

bool DisplayController_SetBrightness(uint8_t brightness)
{
    return DisplayModel_SetBrightness(brightness) &&
           TM1628_SetBrightness(brightness);
}

bool DisplayController_SetTestPattern(const uint16_t segments[6],
    uint8_t top_led_mask, uint8_t bottom_led_mask)
{
    s_page = DISPLAY_PAGE_BOOT;
    s_text_override = false;
    s_numeric_override = false;
    s_message_active = false;
    return DisplayModel_SetRawSegments(segments) &&
           DisplayModel_SetIndicators(top_led_mask, bottom_led_mask);
}

DisplayPage DisplayController_GetPage(void)
{
    return s_page;
}

#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
bool DisplayController_GetDirectionalDiagnostics(
    DirectionalDisplayFollowerDiagnostics *diagnostics)
{
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    const DisplayConditionSnapshot *snapshot =
        MetrologyManager_GetDisplayConditionSnapshot();
    if ((diagnostics == NULL) || (snapshot == NULL)) return false;
    (void)memset(diagnostics, 0, sizeof(*diagnostics));
    diagnostics->display_count = snapshot->display_count;
    diagnostics->last_sample_sequence = snapshot->last_sample_sequence;
    diagnostics->evidence = snapshot->evidence;
    diagnostics->source = (uint8_t)snapshot->source;
    diagnostics->initialized = snapshot->display_domain_valid;
    diagnostics->locked = snapshot->locked;
    diagnostics->last_large_step = snapshot->large_step;
    return true;
#else
    return DirectionalDisplayFollower_GetDiagnostics(
        &s_directional_follower, diagnostics);
#endif
}
#endif
