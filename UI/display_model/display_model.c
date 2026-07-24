#include "display_model.h"

#include "display_formatter.h"

#include <stddef.h>
#include <string.h>

static DisplayModel s_model;

static bool DisplayModel_UpdateSegments(const uint16_t segments[6])
{
    if (memcmp(s_model.digit_segments, segments,
               sizeof(s_model.digit_segments)) == 0)
    {
        return true;
    }
    (void)memcpy(s_model.digit_segments, segments,
                 sizeof(s_model.digit_segments));
    ++s_model.revision;
    return true;
}

void DisplayModel_Init(void)
{
    (void)memset(&s_model, 0, sizeof(s_model));
    s_model.page = DISPLAY_PAGE_BOOT;
    s_model.enabled = true;
    s_model.brightness = 3U;
    s_model.revision = 1U;
}

bool DisplayModel_SetWeight(WeightValue value, uint8_t decimal_places,
    bool show_negative, uint8_t status_flags)
{
    uint16_t segments[6];

    if ((status_flags & WEIGHT_STATUS_CALIBRATION_VALID) == 0U)
    {
        return DisplayModel_SetText6(" noCAL");
    }
    if ((status_flags & WEIGHT_STATUS_OVERLOAD) != 0U)
    {
        return DisplayModel_SetText6("    OL");
    }
    return DisplayFormatter_FormatWeight(value, decimal_places, show_negative,
                                         segments) &&
           DisplayModel_UpdateSegments(segments);
}

bool DisplayModel_SetText6(const char text[6])
{
    uint16_t segments[6];

    return DisplayFormatter_FormatText6(text, segments) &&
           DisplayModel_UpdateSegments(segments);
}

bool DisplayModel_SetBatteryMv(uint32_t battery_mv)
{
    uint16_t segments[6];

    return DisplayFormatter_FormatBatteryMv(battery_mv, segments) &&
           DisplayModel_UpdateSegments(segments);
}

bool DisplayModel_SetMessage(const char text[6], uint32_t duration_ms)
{
    (void)duration_ms;
    return DisplayModel_SetPage(DISPLAY_PAGE_MESSAGE) &&
           DisplayModel_SetText6(text);
}

bool DisplayModel_SetPage(DisplayPage page)
{
    if ((uint32_t)page > (uint32_t)DISPLAY_PAGE_FAULT)
    {
        return false;
    }
    if (s_model.page != page)
    {
        s_model.page = page;
        ++s_model.revision;
    }
    return true;
}

bool DisplayModel_SetIndicators(uint8_t top_led_mask,
                                uint8_t bottom_led_mask)
{
    if (((top_led_mask | bottom_led_mask) & 0xC0U) != 0U)
    {
        return false;
    }
    if ((s_model.top_led_mask != top_led_mask) ||
        (s_model.bottom_led_mask != bottom_led_mask))
    {
        s_model.top_led_mask = top_led_mask;
        s_model.bottom_led_mask = bottom_led_mask;
        ++s_model.revision;
    }
    return true;
}

bool DisplayModel_SetBrightness(uint8_t brightness)
{
    if (brightness > 7U)
    {
        return false;
    }
    if (s_model.brightness != brightness)
    {
        s_model.brightness = brightness;
        ++s_model.revision;
    }
    return true;
}

bool DisplayModel_SetEnabled(bool enabled)
{
    if (s_model.enabled != enabled)
    {
        s_model.enabled = enabled;
        ++s_model.revision;
    }
    return true;
}

bool DisplayModel_SetRawSegments(const uint16_t segments[6])
{
    return (segments != NULL) && DisplayModel_UpdateSegments(segments);
}

const DisplayModel *DisplayModel_Get(void)
{
    return &s_model;
}
