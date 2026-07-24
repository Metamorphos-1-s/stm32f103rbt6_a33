#include "display_controller.h"

#include "battery_adc.h"
#include "bsp_time.h"
#include "display_model.h"
#include "metrology_manager.h"
#include "system_context.h"
#include "tm1628.h"
#include "tm1628_board_map.h"

#include <stddef.h>
#include <string.h>

#define SEGMENT_BIT(segment) ((uint16_t)(1U << (uint8_t)(segment)))

static DisplayPage s_page;
static DisplayPage s_restore_page;
static uint32_t s_message_start_ms;
static uint32_t s_message_duration_ms;
static uint32_t s_applied_revision;
static char s_text[6];
static bool s_text_override;
static bool s_message_active;
static bool s_initialized;

static void DisplayController_BuildIndicators(const WeightSnapshot *snapshot,
    uint8_t *top, uint8_t *bottom)
{
    uint32_t flags = (snapshot != NULL) ? snapshot->status_flags : 0U;

    *top = 0U;
    *bottom = 0U;
    if (s_page == DISPLAY_PAGE_NET) *top |= DISPLAY_TOP_LED_NET;
    if (s_page == DISPLAY_PAGE_GROSS) *top |= DISPLAY_TOP_LED_GROSS;
    if (s_page == DISPLAY_PAGE_TARE) *top |= DISPLAY_TOP_LED_TARE;
    if (s_page == DISPLAY_PAGE_BATTERY) *top |= DISPLAY_TOP_LED_BATTERY;
    if ((flags & WEIGHT_STATUS_STABLE) != 0U) *top |= DISPLAY_TOP_LED_STABLE;
    if ((flags & WEIGHT_STATUS_ZERO) != 0U) *top |= DISPLAY_TOP_LED_ZERO;
    if ((flags & WEIGHT_STATUS_CALIBRATION_VALID) == 0U)
        *bottom |= DISPLAY_BOTTOM_LED_NOCAL;
    if ((flags & WEIGHT_STATUS_TARE_ACTIVE) != 0U)
        *bottom |= DISPLAY_BOTTOM_LED_TARE;
    if ((flags & WEIGHT_STATUS_OVERLOAD) != 0U)
        *bottom |= DISPLAY_BOTTOM_LED_OVERLOAD;
    if ((s_page == DISPLAY_PAGE_MENU) || (s_page == DISPLAY_PAGE_EDIT) ||
        (s_page == DISPLAY_PAGE_CALIBRATION))
        *bottom |= DISPLAY_BOTTOM_LED_MENU;
}

static bool DisplayController_BuildModel(void)
{
    const SystemContext *context = SystemContext_Get();
    const WeightSnapshot *snapshot = MetrologyManager_GetSnapshot();
    const BatteryAdcState *battery = BatteryAdc_GetState();
    uint8_t top;
    uint8_t bottom;
    uint32_t flags = (snapshot != NULL) ? snapshot->status_flags : 0U;
    uint8_t decimals = (context != NULL) ?
        context->config.metrology.decimal_places : 0U;

    (void)DisplayModel_SetPage(s_page);
    DisplayController_BuildIndicators(snapshot, &top, &bottom);
    if (!DisplayModel_SetIndicators(top, bottom))
    {
        return false;
    }
    if (s_text_override)
    {
        return DisplayModel_SetText6(s_text);
    }
    switch (s_page)
    {
        case DISPLAY_PAGE_NET:
            return DisplayModel_SetWeight((snapshot != NULL) ?
                snapshot->net_weight : 0, decimals, true, (uint8_t)flags);
        case DISPLAY_PAGE_GROSS:
            return DisplayModel_SetWeight((snapshot != NULL) ?
                snapshot->gross_weight : 0, decimals, true, (uint8_t)flags);
        case DISPLAY_PAGE_TARE:
            return DisplayModel_SetWeight((snapshot != NULL) ?
                snapshot->tare_weight : 0, decimals, true, (uint8_t)flags);
        case DISPLAY_PAGE_BATTERY:
            return (battery != NULL) && battery->valid ?
                DisplayModel_SetBatteryMv(battery->battery_mv) :
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
    s_restore_page = s_page;
    s_message_active = false;
    s_text_override = false;
    s_applied_revision = 0U;
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

    if (!s_initialized || (state == APP_STATE_DIAGNOSTIC))
    {
        return;
    }
    if (state == APP_STATE_SELF_TEST)
    {
        (void)DisplayController_ApplyModel();
        return;
    }
    if (s_message_active &&
        ((uint32_t)(BSP_TimeNowMs() - s_message_start_ms) >=
         s_message_duration_ms))
    {
        s_message_active = false;
        s_text_override = false;
        s_page = s_restore_page;
    }
    if (!DisplayController_BuildModel())
    {
        return;
    }
    (void)DisplayController_ApplyModel();
}

void DisplayController_SetPage(DisplayPage page)
{
    if ((uint32_t)page <= (uint32_t)DISPLAY_PAGE_FAULT)
    {
        s_page = page;
        s_text_override = false;
        s_message_active = false;
    }
}

void DisplayController_ShowMessage(const char text[6], uint32_t duration_ms)
{
    if ((text == NULL) || (duration_ms == 0U))
    {
        return;
    }
    s_restore_page = s_page;
    (void)memcpy(s_text, text, 6U);
    s_text_override = true;
    s_message_active = true;
    s_message_start_ms = BSP_TimeNowMs();
    s_message_duration_ms = duration_ms;
    s_page = DISPLAY_PAGE_MESSAGE;
}

bool DisplayController_SetTextPage(DisplayPage page, const char text[6])
{
    if ((text == NULL) || ((uint32_t)page > (uint32_t)DISPLAY_PAGE_FAULT))
    {
        return false;
    }
    (void)memcpy(s_text, text, 6U);
    s_text_override = true;
    s_message_active = false;
    s_page = page;
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
    s_message_active = false;
    return DisplayModel_SetRawSegments(segments) &&
           DisplayModel_SetIndicators(top_led_mask, bottom_led_mask);
}

DisplayPage DisplayController_GetPage(void)
{
    return s_page;
}
