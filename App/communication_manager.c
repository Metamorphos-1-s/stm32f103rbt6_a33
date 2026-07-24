#include "communication_manager.h"

#include "bsp_rtu_timer.h"
#include "fault_manager.h"
#include "modbus_register_model.h"
#include "modbus_rtu_framer.h"
#include "modbus_rtu_server.h"
#include "modbus_rtu_timing.h"
#include "persistence_manager.h"
#include "storage_power_guard.h"
#include "system_context.h"
#include "uart2_dma_transport.h"

#include <stddef.h>

static CommunicationManagerState s_state;
static CommunicationConfig s_active;
static CommunicationConfig s_candidate;
static CommunicationConfig s_rollback;
static ModbusRtuTiming s_timing;
static bool s_apply_requested;
static bool s_save_deferred;
static bool s_storage_was_busy;

static BspUart2Config ToUartConfig(const CommunicationConfig *config)
{
    BspUart2Config uart;
    uart.baud_rate = config->baud_rate;
    uart.parity = config->parity;
    uart.stop_bits = config->stop_bits;
    return uart;
}

static bool IsCommunicationValid(const CommunicationConfig *config)
{
    ModbusRtuTiming timing;
    return (config != NULL) && (config->modbus_address >= 1U) &&
        (config->modbus_address <= 247U) &&
        (config->protocol_mode < PROTOCOL_MODE_COUNT) &&
        (config->word_order < MODBUS_WORD_ORDER_COUNT) &&
        (config->response_delay_ms <= 1000U) &&
        (config->broadcast_write_policy == 0U) &&
        ModbusRtuTiming_Calculate(config->baud_rate, config->parity,
            config->stop_bits, &timing);
}

static bool StartStack(const CommunicationConfig *config)
{
    BspUart2Config uart = ToUartConfig(config);
    if (!ModbusRtuTiming_Calculate(config->baud_rate, config->parity,
                                  config->stop_bits, &s_timing) ||
        !BSP_RtuTimerInit() || !Uart2DmaTransport_Init(&uart) ||
        !ModbusRtuFramer_Init(&s_timing) ||
        !ModbusRtuServer_Init(config))
        return false;
    return true;
}

bool CommunicationManager_Init(const CommunicationConfig *config)
{
    if (!IsCommunicationValid(config))
    {
        s_state = COMM_STATE_ERROR;
        return false;
    }
    s_active = *config;
    s_apply_requested = false;
    s_save_deferred = false;
    s_storage_was_busy = false;
    if (config->protocol_mode != PROTOCOL_MODE_MODBUS_RTU)
    {
        s_state = COMM_STATE_DISABLED;
        return true;
    }
    s_state = COMM_STATE_STARTING;
    if (!StartStack(config))
    {
        s_state = COMM_STATE_ERROR;
        FaultManager_Set(FAULT_UART2_DMA_INIT);
        return false;
    }
    s_state = COMM_STATE_RUNNING;
    return true;
}

CommandResult CommunicationManager_RequestApply(void)
{
    if (s_apply_requested || (s_state == COMM_STATE_ERROR) ||
        PersistenceManager_IsBusy()) return COMMAND_RESULT_BUSY;
    if (!ModbusRegisterModel_GetPendingCommunication(&s_candidate) ||
        !IsCommunicationValid(&s_candidate)) return COMMAND_RESULT_INVALID_ARGUMENT;
    s_apply_requested = true;
    return COMMAND_RESULT_ACCEPTED;
}

CommandResult CommunicationManager_RequestDeferredSave(void)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
    return COMMAND_RESULT_STORAGE_UNAVAILABLE;
#else
    if (s_save_deferred || PersistenceManager_IsBusy()) return COMMAND_RESULT_BUSY;
    if (!StoragePowerGuard_CanContinueFlashOperation())
        return COMMAND_RESULT_POWER_UNSAFE;
    s_save_deferred = true;
    return COMMAND_RESULT_ACCEPTED;
#endif
}

static bool CommitCandidate(void)
{
    const SystemContext *context = SystemContext_Get();
    DeviceConfig updated;
    if (context == NULL) return false;
    updated = context->config;
    s_candidate.pending_apply = false;
    updated.communication = s_candidate;
    if (!SystemContext_ApplyConfig(&updated, true)) return false;
    s_active = s_candidate;
    return ModbusRegisterModel_CompleteCommunicationApply(&s_active);
}

void CommunicationManager_Process(void)
{
    bool storage_busy = PersistenceManager_IsBusy();
    Uart2DmaTransport_Process();
    if ((s_state == COMM_STATE_RUNNING) ||
        (s_state == COMM_STATE_RESPONSE_ACTIVE) ||
        (s_state == COMM_STATE_WAIT_OLD_RESPONSE_COMPLETE))
    {
        ModbusRtuFramer_Process();
        ModbusRtuServer_Process();
    }
    if (storage_busy && !s_storage_was_busy)
    {
        ModbusRtuServer_Suspend();
        s_storage_was_busy = true;
        s_state = COMM_STATE_SUSPENDED_STORAGE;
        return;
    }
    if (!storage_busy && s_storage_was_busy)
    {
        Uart2DmaTransport_DiscardPending();
        ModbusRtuFramer_Reset();
        (void)ModbusRtuServer_Resume(&s_active);
        s_storage_was_busy = false;
        s_state = COMM_STATE_RUNNING;
        return;
    }
    switch (s_state)
    {
        case COMM_STATE_RUNNING:
            if (ModbusRtuServer_IsBusy()) s_state = COMM_STATE_RESPONSE_ACTIVE;
            else if (s_apply_requested) s_state = COMM_STATE_APPLY_PENDING;
            else if (s_save_deferred)
            {
                s_save_deferred = false;
                (void)PersistenceManager_RequestSave();
            }
            break;
        case COMM_STATE_RESPONSE_ACTIVE:
            if (!ModbusRtuServer_IsBusy())
                s_state = s_apply_requested ?
                    COMM_STATE_WAIT_OLD_RESPONSE_COMPLETE : COMM_STATE_RUNNING;
            break;
        case COMM_STATE_APPLY_PENDING:
            s_state = COMM_STATE_WAIT_OLD_RESPONSE_COMPLETE;
            break;
        case COMM_STATE_WAIT_OLD_RESPONSE_COMPLETE:
            if (!ModbusRtuServer_IsBusy())
            {
                s_rollback = s_active;
                s_state = COMM_STATE_STOP_OLD_DMA;
            }
            break;
        case COMM_STATE_STOP_OLD_DMA:
            ModbusRtuServer_Suspend();
            Uart2DmaTransport_Suspend();
            ModbusRtuFramer_Reset();
            s_state = COMM_STATE_APPLY_NEW_UART;
            break;
        case COMM_STATE_APPLY_NEW_UART:
            if (StartStack(&s_candidate) && CommitCandidate())
                s_state = COMM_STATE_RESTART_RX;
            else s_state = COMM_STATE_ROLLBACK;
            break;
        case COMM_STATE_RESTART_RX:
            s_apply_requested = false;
            s_state = (s_active.protocol_mode == PROTOCOL_MODE_MODBUS_RTU) ?
                COMM_STATE_RUNNING : COMM_STATE_DISABLED;
            break;
        case COMM_STATE_ROLLBACK:
            if (StartStack(&s_rollback))
            {
                s_active = s_rollback;
                s_apply_requested = false;
                s_state = COMM_STATE_RUNNING;
                FaultManager_Set(FAULT_COMM_CONFIG_APPLY);
            }
            else
            {
                FaultManager_Set(FAULT_MODBUS_TRANSPORT_FATAL);
                s_state = COMM_STATE_ERROR;
            }
            break;
        case COMM_STATE_DISABLED:
        case COMM_STATE_STARTING:
        case COMM_STATE_SUSPENDED_STORAGE:
        case COMM_STATE_ERROR:
        default:
            break;
    }
}

CommunicationManagerState CommunicationManager_GetState(void) { return s_state; }
const CommunicationConfig *CommunicationManager_GetActiveConfig(void)
{
    return &s_active;
}
