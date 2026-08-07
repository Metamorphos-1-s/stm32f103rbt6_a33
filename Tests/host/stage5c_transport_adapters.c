#include "w02_uart.h"
#include "w02_pwrkey.h"

#include <string.h>

static bool s_ready;
static bool s_busy;
static bool s_reject_tx;
static uint16_t s_last_tx_length;
static uint8_t s_last_tx[256];
static W02UartEvents s_events;

void W02PwrKey_Init(void) {}
bool W02PwrKey_RequestPulse(uint32_t low_time_ms)
{
    (void)low_time_ms;
    return true;
}
void W02PwrKey_Process(void) {}
bool W02PwrKey_IsBusy(void) { return false; }
W02PwrKeyState W02PwrKey_GetState(void) { return W02_PWRKEY_IDLE; }
bool W02PwrKey_IsPulseDurationValid(uint32_t low_time_ms)
{
    (void)low_time_ms;
    return true;
}

void Stage5C_FakeReset(void)
{
    s_ready = true;
    s_busy = false;
    s_reject_tx = false;
    s_last_tx_length = 0U;
    (void)memset(s_last_tx, 0, sizeof(s_last_tx));
    (void)memset(&s_events, 0, sizeof(s_events));
}

void Stage5C_FakeCompleteTx(void)
{
    s_busy = false;
    ++s_events.tx_complete_count;
}

void Stage5C_FakeRejectTx(bool reject) { s_reject_tx = reject; }
uint16_t Stage5C_FakeTxLength(void) { return s_last_tx_length; }
uint8_t Stage5C_FakeTxByte(uint16_t index)
{
    return (index < s_last_tx_length) ? s_last_tx[index] : 0U;
}
void Stage5C_FakeSetReady(bool ready) { s_ready = ready; }
void Stage5C_FakeUartError(void)
{
    s_busy = false;
    ++s_events.uart_error_count;
}

bool W02Uart_Init(uint32_t baud_rate)
{
    (void)baud_rate;
    s_ready = true;
    return true;
}

bool W02Uart_StartTx(const uint8_t *data, uint16_t length)
{
    if (!s_ready || s_busy || s_reject_tx || (data == NULL) ||
        (length == 0U) || (length > sizeof(s_last_tx))) return false;
    (void)memcpy(s_last_tx, data, length);
    s_last_tx_length = length;
    s_busy = true;
    return true;
}

bool W02Uart_IsTxBusy(void) { return s_busy; }
bool W02Uart_IsReady(void) { return s_ready; }

void W02Uart_GetEvents(W02UartEvents *events)
{
    if (events != NULL) *events = s_events;
}
