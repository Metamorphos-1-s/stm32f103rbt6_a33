#ifndef DISPLAY_CONTROLLER_H
#define DISPLAY_CONTROLLER_H

#include "display_types.h"

#include <stdbool.h>
#include <stdint.h>

bool DisplayController_Init(void);
void DisplayController_Process20ms(void);
void DisplayController_SetPage(DisplayPage page);
void DisplayController_ShowMessage(const char text[6], uint32_t duration_ms);
bool DisplayController_SetTextPage(DisplayPage page, const char text[6]);
bool DisplayController_SetBrightness(uint8_t brightness);
bool DisplayController_SetTestPattern(const uint16_t segments[6],
    uint8_t top_led_mask, uint8_t bottom_led_mask);
DisplayPage DisplayController_GetPage(void);

#endif /* DISPLAY_CONTROLLER_H */
