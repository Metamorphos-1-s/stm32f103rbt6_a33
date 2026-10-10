#ifndef DISPLAY_FORMATTER_H
#define DISPLAY_FORMATTER_H

#include "weight_types.h"
#include "project_config.h"

#include <stdbool.h>
#include <stdint.h>

bool DisplayFormatter_EncodeCharacter(char character,
                                      uint16_t *logical_segments);
bool DisplayFormatter_FormatWeight(WeightValue value, uint8_t decimal_places,
    bool show_negative, uint16_t segments[6]);
bool DisplayFormatter_FormatText6(const char text[6], uint16_t segments[6]);
bool DisplayFormatter_FormatBatteryMv(uint32_t battery_mv,
                                      uint16_t segments[6]);
#if (A33_ENABLE_STAGE5P_UNIFIED != 0U)
bool DisplayFormatter_FormatBatteryPercent(uint8_t percent,
                                           uint16_t segments[6]);
#endif

#endif /* DISPLAY_FORMATTER_H */
