#ifndef DISPLAY_TYPES_H
#define DISPLAY_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    DISPLAY_PAGE_BOOT = 0,
    DISPLAY_PAGE_NET,
    DISPLAY_PAGE_GROSS,
    DISPLAY_PAGE_TARE,
    DISPLAY_PAGE_BATTERY,
    DISPLAY_PAGE_STATUS,
    DISPLAY_PAGE_MENU,
    DISPLAY_PAGE_EDIT,
    DISPLAY_PAGE_CALIBRATION,
    DISPLAY_PAGE_MESSAGE,
    DISPLAY_PAGE_FAULT
} DisplayPage;

#define DISPLAY_TOP_LED_NET       (1U << 0)
#define DISPLAY_TOP_LED_GROSS     (1U << 1)
#define DISPLAY_TOP_LED_TARE      (1U << 2)
#define DISPLAY_TOP_LED_STABLE    (1U << 3)
#define DISPLAY_TOP_LED_ZERO      (1U << 4)
#define DISPLAY_TOP_LED_BATTERY   (1U << 5)

#define DISPLAY_BOTTOM_LED_NOCAL  (1U << 0)
#define DISPLAY_BOTTOM_LED_TARE   (1U << 1)
#define DISPLAY_BOTTOM_LED_OVERLOAD (1U << 2)
#define DISPLAY_BOTTOM_LED_MENU   (1U << 3)
#define DISPLAY_BOTTOM_LED_COMM   (1U << 4)
#define DISPLAY_BOTTOM_LED_BATTERY (1U << 5)

typedef struct
{
    DisplayPage page;
    uint16_t digit_segments[6];
    uint8_t top_led_mask;
    uint8_t bottom_led_mask;
    uint8_t brightness;
    bool enabled;
    uint32_t revision;
} DisplayModel;

#endif /* DISPLAY_TYPES_H */
