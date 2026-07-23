#ifndef STAGE2B_DISPLAY_FONT_H
#define STAGE2B_DISPLAY_FONT_H

#include <stdbool.h>
#include <stdint.h>

bool Stage2B_FontEncode(char character, uint16_t *segment_mask);
bool Stage2B_FormatHex24(uint32_t raw24, char text[6]);
bool Stage2B_FormatBatteryMv(uint32_t battery_mv, bool valid,
                             char text[6], uint8_t *decimal_point_mask);
bool Stage2B_FormatKeyMask(uint8_t key_mask, char text[6]);

#endif /* STAGE2B_DISPLAY_FONT_H */
