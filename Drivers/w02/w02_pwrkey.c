#include "w02_pwrkey.h"

#include "bsp_gpio.h"
#include "bsp_time.h"
#include "event_queue.h"
#include "project_config.h"

#include <stddef.h>

static W02PwrKeyState s_state;
static uint32_t s_assert_start_ms;
static uint32_t s_requested_low_ms;

static void W02PwrKey_PushEvent(EventType type, uint32_t arg0);

void W02PwrKey_Init(void)
{
    BSP_W02_PwrKeyRelease();
    s_assert_start_ms = 0U;
    s_requested_low_ms = 0U;
    s_state = W02_PWRKEY_IDLE;
}

bool W02PwrKey_RequestPulse(uint32_t low_time_ms)
{
    if (!W02PwrKey_IsPulseDurationValid(low_time_ms) ||
        (s_state != W02_PWRKEY_IDLE) || !BSP_W02_PwrKeyAssertLow())
    {
        return false;
    }

    s_requested_low_ms = low_time_ms;
    s_assert_start_ms = BSP_TimeNowMs();
    s_state = W02_PWRKEY_ASSERTED;
    return true;
}

void W02PwrKey_Process(void)
{
    uint32_t now_ms;

    if (s_state != W02_PWRKEY_ASSERTED)
    {
        return;
    }

    now_ms = BSP_TimeNowMs();
    if (BSP_TimeElapsed(now_ms, s_assert_start_ms,
                        W02_PWRKEY_HARD_LIMIT_MS))
    {
        BSP_W02_PwrKeyRelease();
        s_state = W02_PWRKEY_ERROR;
        W02PwrKey_PushEvent(EVENT_DRIVER_ERROR, 1U);
    }
    else if (BSP_TimeElapsed(now_ms, s_assert_start_ms,
                             s_requested_low_ms))
    {
        BSP_W02_PwrKeyRelease();
        s_state = W02_PWRKEY_IDLE;
        W02PwrKey_PushEvent(EVENT_W02_PWRKEY_PULSE_DONE,
                            s_requested_low_ms);
    }
}

bool W02PwrKey_IsBusy(void)
{
    return s_state == W02_PWRKEY_ASSERTED;
}

W02PwrKeyState W02PwrKey_GetState(void)
{
    return s_state;
}

bool W02PwrKey_IsPulseDurationValid(uint32_t low_time_ms)
{
    return (low_time_ms >= W02_PWRKEY_MIN_PULSE_MS) &&
           (low_time_ms <= W02_PWRKEY_MAX_PULSE_MS);
}

static void W02PwrKey_PushEvent(EventType type, uint32_t arg0)
{
    AppEvent event;

    event.type = type;
    event.timestamp_ms = BSP_TimeNowMs();
    event.arg0 = arg0;
    event.arg1 = 0U;
    event.source = NULL;
    (void)EventQueue_Push(&event);
}
