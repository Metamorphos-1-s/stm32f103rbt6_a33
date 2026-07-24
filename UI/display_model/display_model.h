#ifndef DISPLAY_MODEL_H
#define DISPLAY_MODEL_H

#include "display_types.h"
#include "weight_types.h"

#include <stdbool.h>
#include <stdint.h>

void DisplayModel_Init(void);
bool DisplayModel_SetWeight(WeightValue value, uint8_t decimal_places,
    bool show_negative, uint8_t status_flags);
bool DisplayModel_SetText6(const char text[6]);
bool DisplayModel_SetBatteryMv(uint32_t battery_mv);
bool DisplayModel_SetMessage(const char text[6], uint32_t duration_ms);
bool DisplayModel_SetPage(DisplayPage page);
bool DisplayModel_SetIndicators(uint8_t top_led_mask,
                                uint8_t bottom_led_mask);
bool DisplayModel_SetBrightness(uint8_t brightness);
bool DisplayModel_SetEnabled(bool enabled);
bool DisplayModel_SetRawSegments(const uint16_t segments[6]);
const DisplayModel *DisplayModel_Get(void);

#endif /* DISPLAY_MODEL_H */
