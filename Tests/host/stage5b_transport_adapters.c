#include "stage5b_transport_adapters.h"

#include "bsp_rtu_timer.h"
#include "bsp_uart3_dma.h"
#include "bsp_uart_dma.h"
#include "fault_manager.h"
#include "persistence_manager.h"
#include "system_context.h"
#include "uart2_dma_transport.h"

#include <stddef.h>
#include <string.h>

static uint32_t s_now_us;
static bool s_tx_complete;
static bool s_de;
static bool s_timer_elapsed;
static bool s_persistence_busy;
static CommandResult s_save_result;
static bool s_apply_config_result;
static Uart2DmaTransportStatistics s_transport;

uint32_t BSP_TimeNowMs(void) { return 0U; }
uint32_t BSP_TimeNowUs(void) { return s_now_us; }
uint32_t BSP_TimeNowCycles(void) { return s_now_us; }
bool BSP_TimeUsToCycles(uint32_t interval_us, uint32_t *interval_cycles)
{
    if (interval_cycles == NULL) return false;
    *interval_cycles = interval_us;
    return true;
}
void Stage5B_TransportReset(void)
{
    s_now_us = 0U;
    s_tx_complete = false;
    s_de = false;
    s_timer_elapsed = false;
    s_persistence_busy = false;
    s_save_result = COMMAND_RESULT_ACCEPTED;
    s_apply_config_result = true;
    (void)memset(&s_transport, 0, sizeof(s_transport));
}

void Stage5B_SetNowUs(uint32_t now_us) { s_now_us = now_us; }
void Stage5B_SetTxComplete(bool complete) { s_tx_complete = complete; }
bool Stage5B_IsDeAsserted(void) { return s_de; }
void Stage5B_SetDmaWritePosition(uint16_t position)
{
    s_transport.dma_write_position = position;
}

void BSP_Rs485SetTransmit(bool transmit) { s_de = transmit; }
BspUartDmaResult BSP_Uart2DmaStartTx(const uint8_t *data, uint16_t length)
{
    return ((data != NULL) && (length != 0U)) ?
        BSP_UART_DMA_OK : BSP_UART_DMA_INVALID_ARGUMENT;
}
void BSP_Uart2DmaAbortTx(void) { s_tx_complete = true; }
bool BSP_Uart2IsTxCompletelyFinished(void) { return s_tx_complete; }

bool BSP_RtuTimerStartUs(uint32_t delay_us)
{
    s_timer_elapsed = false;
    return delay_us != 0U;
}
bool BSP_RtuTimerInit(void) { return true; }
void BSP_RtuTimerStop(void) { s_timer_elapsed = false; }
bool BSP_RtuTimerTakeElapsed(void)
{
    bool value = s_timer_elapsed;
    s_timer_elapsed = false;
    return value;
}

bool Uart2DmaTransport_TakeReceiveError(void) { return false; }
void Uart2DmaTransport_DiscardPending(void) { }
bool Uart2DmaTransport_TryReadByte(uint8_t *byte)
{
    (void)byte;
    return false;
}
bool Uart2DmaTransport_TakeIdleEvent(uint16_t *dma_position,
                                    uint32_t *timestamp_cycles)
{
    (void)dma_position;
    (void)timestamp_cycles;
    return false;
}
const Uart2DmaTransportStatistics *Uart2DmaTransport_GetStatistics(void)
{
    return &s_transport;
}

bool Uart2DmaTransport_Init(const BspUart2Config *config)
{
    return config != NULL;
}
void Uart2DmaTransport_Process(void) { }
void Uart2DmaTransport_Suspend(void) { }

bool BSP_Uart3DmaInit(void) { return true; }
BspUartDmaResult BSP_Uart3DmaStartRx(uint8_t *buffer, uint16_t length)
{ return ((buffer != NULL) && (length != 0U)) ? BSP_UART_DMA_OK : BSP_UART_DMA_INVALID_ARGUMENT; }
BspUartDmaResult BSP_Uart3DmaStartTx(const uint8_t *data, uint16_t length)
{ return ((data != NULL) && (length != 0U)) ? BSP_UART_DMA_OK : BSP_UART_DMA_INVALID_ARGUMENT; }
uint16_t BSP_Uart3DmaGetRxPosition(uint16_t buffer_length)
{ (void)buffer_length; return 0U; }
void BSP_Uart3DmaStopRx(void) { }
void BSP_Uart3DmaAbortTx(void) { }
bool BSP_Uart3IsTxCompletelyFinished(void) { return true; }
void BSP_Uart3EnableIdleInterrupt(void) { }
void BSP_Uart3DisableIdleInterrupt(void) { }
void BSP_Uart3DmaGetEvents(BspUart3DmaEvents *events)
{ if (events != NULL) (void)memset(events, 0, sizeof(*events)); }
bool BSP_Uart3DmaTakeIdleEvent(BspUart3IdleEvent *event)
{ (void)event; return false; }
void BSP_Uart3DmaClearIdleEvents(void) { }
void BSP_Uart3IrqHandler(void) { }
void BSP_Uart3RxDmaIrqHandler(void) { }
void BSP_Uart3TxDmaIrqHandler(void) { }
void BSP_Uart3DmaOnRxHalfComplete(void) { }
void BSP_Uart3DmaOnRxComplete(void) { }
void BSP_Uart3DmaOnTxComplete(void) { }
void BSP_Uart3DmaOnError(uint32_t error_code) { (void)error_code; }

void Stage5B_SetPersistenceBusy(bool busy) { s_persistence_busy = busy; }
void Stage5B_SetPersistenceSaveResult(CommandResult result) { s_save_result = result; }
void Stage5B_SetApplyConfigResult(bool result) { s_apply_config_result = result; }
bool PersistenceManager_IsBusy(void) { return s_persistence_busy; }
CommandResult PersistenceManager_RequestSave(void)
{
    return s_save_result;
}
bool SystemContext_ApplyConfig(const DeviceConfig *config, bool dirty)
{
    (void)dirty;
    return (config != NULL) && s_apply_config_result;
}
void FaultManager_Set(FaultCode fault) { (void)fault; }
