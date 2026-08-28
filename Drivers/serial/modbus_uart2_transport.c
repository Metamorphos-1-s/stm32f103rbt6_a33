#include "modbus_uart2_transport.h"

#include "rs485_tx_controller.h"
#include "uart2_dma_transport.h"

#include <stddef.h>

static void Process(void *context)
{
    (void)context;
    Uart2DmaTransport_Process();
    Rs485TxController_Process();
}

static bool TryReadByte(void *context, uint8_t *byte)
{
    (void)context;
    return Uart2DmaTransport_TryReadByte(byte);
}

static bool TakeIdleEvent(void *context, uint16_t *position,
                          uint32_t *timestamp_cycles)
{
    (void)context;
    return Uart2DmaTransport_TakeIdleEvent(position, timestamp_cycles);
}

static bool TakeReceiveError(void *context)
{
    (void)context;
    return Uart2DmaTransport_TakeReceiveError();
}

static void DiscardPending(void *context)
{
    (void)context;
    Uart2DmaTransport_DiscardPending();
}

static uint16_t GetRxPosition(void *context)
{
    (void)context;
    return Uart2DmaTransport_GetStatistics()->dma_write_position;
}

static bool StartTx(void *context, const uint8_t *data, uint16_t length)
{
    (void)context;
    return Rs485TxController_Start(data, length);
}

static bool TakeTxCompleted(void *context)
{
    (void)context;
    return Rs485TxController_TakeCompleted();
}

static bool TakeTxError(void *context, bool *timeout)
{
    (void)context;
    if ((timeout == NULL) ||
        (Rs485TxController_GetState() != RS485_TX_ERROR)) return false;
    *timeout = Rs485TxController_GetLastError() == RS485_TX_ERROR_TIMEOUT;
    return true;
}

static void AbortTx(void *context)
{
    (void)context;
    Rs485TxController_Abort();
}

static const ModbusRtuTransport s_transport = {
    NULL, Process, TryReadByte, TakeIdleEvent, TakeReceiveError,
    DiscardPending, GetRxPosition, StartTx, TakeTxCompleted, TakeTxError,
    AbortTx
};

void ModbusUart2Transport_Init(void)
{
    Rs485TxController_Init();
}

const ModbusRtuTransport *ModbusUart2Transport_Get(void)
{
    return &s_transport;
}
