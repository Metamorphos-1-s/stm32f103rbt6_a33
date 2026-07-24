#include "self_test_controller.h"

#include "bsp_time.h"
#include "display_controller.h"
#include "display_formatter.h"
#include "output_gpio.h"
#include "project_config.h"

#include <string.h>

static SelfTestState s_state;
static uint32_t s_enter_ms;
static uint8_t s_step;

static void SelfTestController_SetState(SelfTestState state, uint32_t now)
{
    s_state = state;
    s_enter_ms = now;
    s_step = 0U;
}

void SelfTestController_Init(void)
{
    s_state = SELF_TEST_IDLE;
    s_enter_ms = 0U;
    s_step = 0U;
}

bool SelfTestController_Begin(void)
{
    uint16_t blank[6] = {0U, 0U, 0U, 0U, 0U, 0U};

    if ((s_state != SELF_TEST_IDLE) && (s_state != SELF_TEST_COMPLETE))
    {
        return false;
    }
    (void)OutputGpio_Set(OUTPUT_INTERNAL_BUZZER, false);
    if (!DisplayController_SetTestPattern(blank, 0U, 0U))
    {
        s_state = SELF_TEST_FAILED;
        return false;
    }
    SelfTestController_SetState(SELF_TEST_DISPLAY_CLEAR, BSP_TimeNowMs());
    return true;
}

void SelfTestController_Process10ms(void)
{
    uint32_t now = BSP_TimeNowMs();
    uint32_t elapsed = now - s_enter_ms;
    uint16_t segments[6] = {0U, 0U, 0U, 0U, 0U, 0U};
    uint16_t eight = 0U;

    switch (s_state)
    {
        case SELF_TEST_DISPLAY_CLEAR:
            if (elapsed >= SELF_TEST_CLEAR_MS)
                SelfTestController_SetState(SELF_TEST_DIGIT_WALK, now);
            break;
        case SELF_TEST_DIGIT_WALK:
            if (elapsed >= SELF_TEST_STEP_MS)
            {
                (void)DisplayFormatter_EncodeCharacter('8', &eight);
                segments[s_step] = eight;
                (void)DisplayController_SetTestPattern(segments, 0U, 0U);
                s_enter_ms = now;
                if (++s_step >= 6U)
                    SelfTestController_SetState(SELF_TEST_UP_LED_WALK, now);
            }
            break;
        case SELF_TEST_UP_LED_WALK:
            if (elapsed >= SELF_TEST_STEP_MS)
            {
                (void)DisplayController_SetTestPattern(segments,
                    (uint8_t)(1U << s_step), 0U);
                s_enter_ms = now;
                if (++s_step >= 6U)
                    SelfTestController_SetState(SELF_TEST_DOWN_LED_WALK, now);
            }
            break;
        case SELF_TEST_DOWN_LED_WALK:
            if (elapsed >= SELF_TEST_STEP_MS)
            {
                (void)DisplayController_SetTestPattern(segments, 0U,
                    (uint8_t)(1U << s_step));
                s_enter_ms = now;
                if (++s_step >= 6U)
                {
#if (SELF_TEST_INTERNAL_BEEP_ENABLED != 0U)
                    (void)OutputGpio_Set(OUTPUT_INTERNAL_BUZZER, true);
#endif
                    SelfTestController_SetState(SELF_TEST_INTERNAL_BEEP, now);
                }
            }
            break;
        case SELF_TEST_INTERNAL_BEEP:
            if (elapsed >= SELF_TEST_BEEP_MS)
            {
                (void)OutputGpio_Set(OUTPUT_INTERNAL_BUZZER, false);
                (void)DisplayController_SetTextPage(DISPLAY_PAGE_BOOT,
                                                    "A33 4A");
                SelfTestController_SetState(SELF_TEST_SHOW_VERSION, now);
            }
            break;
        case SELF_TEST_SHOW_VERSION:
            if (elapsed >= SELF_TEST_VERSION_MS)
                SelfTestController_SetState(SELF_TEST_COMPLETE, now);
            break;
        case SELF_TEST_IDLE:
        case SELF_TEST_COMPLETE:
        case SELF_TEST_FAILED:
        default:
            break;
    }
}

void SelfTestController_Cancel(void)
{
    (void)OutputGpio_Set(OUTPUT_INTERNAL_BUZZER, false);
    s_state = SELF_TEST_IDLE;
}

SelfTestState SelfTestController_GetState(void)
{
    return s_state;
}
