#include "rs485_tx_controller.h"

#include "bsp_time.h"
#include "bsp_uart_dma.h"

#include <stddef.h>

#define RS485_TX_TIMEOUT_US 200000U

static Rs485TxState s_state;
static const uint8_t *s_data;
static uint16_t s_length;
static uint32_t s_state_enter_us;
static uint32_t s_tx_start_us;
static bool s_completed;

static bool Elapsed(uint32_t now, uint32_t start, uint32_t interval)
{
    return (uint32_t)(now - start) >= interval;
}

void Rs485TxController_Init(void)
{
    BSP_Rs485SetTransmit(false);
    s_state = RS485_TX_IDLE;
    s_completed = false;
}

bool Rs485TxController_Start(const uint8_t *data, uint16_t length)
{
    if ((s_state != RS485_TX_IDLE) || (data == NULL) || (length == 0U))
        return false;
    s_data = data;
    s_length = length;
    s_state = RS485_TX_ASSERT_DE;
    s_completed = false;
    return true;
}

void Rs485TxController_Process(void)
{
    uint32_t now = BSP_TimeNowUs();
    if ((s_state >= RS485_TX_DMA_ACTIVE) &&
        (s_state <= RS485_TX_WAIT_DE_HOLD) &&
        Elapsed(now, s_tx_start_us, RS485_TX_TIMEOUT_US))
    {
        Rs485TxController_Abort();
        s_state = RS485_TX_ERROR;
        return;
    }
    switch (s_state)
    {
        case RS485_TX_ASSERT_DE:
            BSP_Rs485SetTransmit(true);
            s_state_enter_us = now;
            s_tx_start_us = now;
            s_state = RS485_TX_WAIT_DE_SETUP;
            break;
        case RS485_TX_WAIT_DE_SETUP:
            if (Elapsed(now, s_state_enter_us, RS485_DE_SETUP_US))
            {
                if (BSP_Uart2DmaStartTx(s_data, s_length) == BSP_UART_DMA_OK)
                    s_state = RS485_TX_DMA_ACTIVE;
                else
                {
                    BSP_Rs485SetTransmit(false);
                    s_state = RS485_TX_ERROR;
                }
            }
            break;
        case RS485_TX_DMA_ACTIVE:
            if (BSP_Uart2IsTxCompletelyFinished())
                s_state = RS485_TX_WAIT_UART_TC;
            break;
        case RS485_TX_WAIT_UART_TC:
            if (BSP_Uart2IsTxCompletelyFinished())
            {
                s_state_enter_us = now;
                s_state = RS485_TX_WAIT_DE_HOLD;
            }
            break;
        case RS485_TX_WAIT_DE_HOLD:
            if (Elapsed(now, s_state_enter_us, RS485_DE_HOLD_US))
                s_state = RS485_TX_RELEASE_DE;
            break;
        case RS485_TX_RELEASE_DE:
            BSP_Rs485SetTransmit(false);
            s_completed = true;
            s_state = RS485_TX_IDLE;
            break;
        case RS485_TX_IDLE:
        case RS485_TX_ERROR:
        default:
            break;
    }
}

void Rs485TxController_Abort(void)
{
    BSP_Uart2DmaAbortTx();
    BSP_Rs485SetTransmit(false);
    s_state = RS485_TX_IDLE;
}

bool Rs485TxController_IsBusy(void) { return s_state != RS485_TX_IDLE; }
bool Rs485TxController_TakeCompleted(void)
{
    bool value = s_completed;
    s_completed = false;
    return value;
}
Rs485TxState Rs485TxController_GetState(void) { return s_state; }
