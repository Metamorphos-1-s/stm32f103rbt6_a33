#include "communication_manager.h"

#include "fault_manager.h"
#include "modbus_register_model.h"
#include "modbus_rtu_framer.h"
#include "modbus_rtu_server.h"
#include "modbus_rtu_timing.h"
#include "modbus_uart2_transport.h"
#include "persistence_manager.h"
#include "storage_power_guard.h"
#include "system_context.h"
#include "uart2_dma_transport.h"
#include "uart3_modbus_transport.h"

#include <stddef.h>

#ifndef A33_ENABLE_STAGE5I_USART3_BRINGUP
#define A33_ENABLE_STAGE5I_USART3_BRINGUP 0
#endif

static CommunicationManagerState s_state;
static CommunicationConfig s_active;
static CommunicationConfig s_candidate;
static CommunicationConfig s_rollback;
static ModbusRtuFramer s_framer2;
static ModbusRtuFramer s_framer3;
static ModbusRtuServer s_server2;
static ModbusRtuServer s_server3;
static ModbusRtuTiming s_timing2;
#if (A33_ENABLE_STAGE5I_USART3_BRINGUP == 0)
static ModbusRtuTiming s_timing3;
#endif
static bool s_apply_requested;
static bool s_save_deferred;
static bool s_storage_was_busy;
static bool s_uart3_enabled;
static bool s_uart3_first;
static uint32_t s_uart2_first_count;
static uint32_t s_uart3_first_count;

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

static bool StartUart2(const CommunicationConfig *config)
{
    BspUart2Config uart = ToUartConfig(config);
    const ModbusRtuTransport *transport = ModbusUart2Transport_Get();
    if (!ModbusRtuTiming_Calculate(config->baud_rate, config->parity,
        config->stop_bits, &s_timing2) || !Uart2DmaTransport_Init(&uart))
        return false;
    ModbusUart2Transport_Init();
    return ModbusRtuFramer_Init(&s_framer2, &s_timing2,
        transport->get_rx_position(transport->context)) &&
        ModbusRtuServer_Init(&s_server2, config, &s_framer2, transport,
                             COMMAND_SOURCE_MODBUS);
}

static bool StartUart3(const CommunicationConfig *config)
{
#if (A33_ENABLE_STAGE5I_USART3_BRINGUP != 0)
    (void)config;
    s_uart3_enabled = false;
    return true;
#else
    const ModbusRtuTransport *transport = Uart3ModbusTransport_Get();
    if (!ModbusRtuTiming_Calculate(115200U, COMM_PARITY_NONE,
        COMM_STOP_BITS_1, &s_timing3) || !Uart3ModbusTransport_Init())
        return false;
    if (!ModbusRtuFramer_Init(&s_framer3, &s_timing3,
        transport->get_rx_position(transport->context)) ||
        !ModbusRtuServer_Init(&s_server3, config, &s_framer3, transport,
                              COMMAND_SOURCE_MODBUS_USART3))
    {
        Uart3ModbusTransport_Suspend();
        return false;
    }
    s_uart3_enabled = true;
    return true;
#endif
}

static bool AnyServerBusy(void)
{
    return ModbusRtuServer_IsBusy(&s_server2) ||
        (s_uart3_enabled && ModbusRtuServer_IsBusy(&s_server3));
}

static void ProcessPort(ModbusRtuFramer *framer, ModbusRtuServer *server,
                        const ModbusRtuTransport *transport)
{
    transport->process(transport->context);
    ModbusRtuFramer_Process(framer, transport);
    ModbusRtuServer_Process(server);
}

static void ProcessTransports(void)
{
    const ModbusRtuTransport *uart2 = ModbusUart2Transport_Get();
#if (A33_ENABLE_STAGE5I_USART3_BRINGUP == 0)
    const ModbusRtuTransport *uart3 = Uart3ModbusTransport_Get();
#endif
    bool service = (s_state == COMM_STATE_RUNNING) ||
        (s_state == COMM_STATE_RESPONSE_ACTIVE) ||
        (s_state == COMM_STATE_WAIT_OLD_RESPONSE_COMPLETE);
    if (!service)
    {
        uart2->process(uart2->context);
#if (A33_ENABLE_STAGE5I_USART3_BRINGUP == 0)
        if (s_uart3_enabled) uart3->process(uart3->context);
#endif
        return;
    }
#if (A33_ENABLE_STAGE5I_USART3_BRINGUP == 0)
    if (s_uart3_enabled && s_uart3_first)
    {
        ++s_uart3_first_count;
        ProcessPort(&s_framer3, &s_server3, uart3);
        ProcessPort(&s_framer2, &s_server2, uart2);
    }
    else
    {
        ++s_uart2_first_count;
        ProcessPort(&s_framer2, &s_server2, uart2);
        if (s_uart3_enabled) ProcessPort(&s_framer3, &s_server3, uart3);
    }
    if (s_uart3_enabled) s_uart3_first = !s_uart3_first;
#else
    ++s_uart2_first_count;
    ProcessPort(&s_framer2, &s_server2, uart2);
#endif
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
    s_uart3_enabled = false;
    s_uart3_first = false;
    s_uart2_first_count = 0U;
    s_uart3_first_count = 0U;
    if (config->protocol_mode != PROTOCOL_MODE_MODBUS_RTU)
    {
        s_state = COMM_STATE_DISABLED;
        return true;
    }
    s_state = COMM_STATE_STARTING;
    if (!StartUart2(config) || !StartUart3(config))
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
    return CommunicationManager_RequestApplyForSource(COMMAND_SOURCE_MODBUS);
}

CommandResult CommunicationManager_RequestApplyForSource(CommandSource source)
{
    if ((uint32_t)source >= (uint32_t)COMMAND_SOURCE_COUNT)
        return COMMAND_RESULT_INVALID_ARGUMENT;
    if (s_apply_requested || (s_state == COMM_STATE_ERROR) ||
        PersistenceManager_IsBusy()) return COMMAND_RESULT_BUSY;
    if (!ModbusRegisterModel_GetPendingCommunicationForSource(source,
        &s_candidate) || !IsCommunicationValid(&s_candidate))
        return COMMAND_RESULT_INVALID_ARGUMENT;
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

static void SuspendServers(void)
{
    ModbusRtuServer_Suspend(&s_server2);
    if (s_uart3_enabled) ModbusRtuServer_Suspend(&s_server3);
}

static void DiscardAndReset(void)
{
    const ModbusRtuTransport *uart2 = ModbusUart2Transport_Get();
#if (A33_ENABLE_STAGE5I_USART3_BRINGUP == 0)
    const ModbusRtuTransport *uart3 = Uart3ModbusTransport_Get();
#endif
    uart2->discard_pending(uart2->context);
    ModbusRtuFramer_Reset(&s_framer2,
        uart2->get_rx_position(uart2->context));
#if (A33_ENABLE_STAGE5I_USART3_BRINGUP == 0)
    if (s_uart3_enabled)
    {
        uart3->discard_pending(uart3->context);
        ModbusRtuFramer_Reset(&s_framer3,
            uart3->get_rx_position(uart3->context));
    }
#endif
}

static bool ResumeServers(const CommunicationConfig *config)
{
    return ModbusRtuServer_Resume(&s_server2, config) &&
        (!s_uart3_enabled || ModbusRtuServer_Resume(&s_server3, config));
}

void CommunicationManager_Process(void)
{
    bool storage_busy = PersistenceManager_IsBusy();
    ProcessTransports();
    if (storage_busy && !s_storage_was_busy)
    {
        SuspendServers();
        s_storage_was_busy = true;
        s_state = COMM_STATE_SUSPENDED_STORAGE;
        return;
    }
    if (!storage_busy && s_storage_was_busy)
    {
        DiscardAndReset();
        (void)ResumeServers(&s_active);
        s_storage_was_busy = false;
        s_state = COMM_STATE_RUNNING;
        return;
    }
    switch (s_state)
    {
        case COMM_STATE_RUNNING:
            if (AnyServerBusy()) s_state = COMM_STATE_RESPONSE_ACTIVE;
            else if (s_apply_requested) s_state = COMM_STATE_APPLY_PENDING;
            else if (s_save_deferred)
            {
                s_save_deferred = false;
                (void)PersistenceManager_RequestSave();
            }
            break;
        case COMM_STATE_RESPONSE_ACTIVE:
            if (!AnyServerBusy())
                s_state = s_apply_requested ?
                    COMM_STATE_WAIT_OLD_RESPONSE_COMPLETE : COMM_STATE_RUNNING;
            break;
        case COMM_STATE_APPLY_PENDING:
            s_state = COMM_STATE_WAIT_OLD_RESPONSE_COMPLETE;
            break;
        case COMM_STATE_WAIT_OLD_RESPONSE_COMPLETE:
            if (!AnyServerBusy())
            {
                s_rollback = s_active;
                s_state = COMM_STATE_STOP_OLD_DMA;
            }
            break;
        case COMM_STATE_STOP_OLD_DMA:
            SuspendServers();
            Uart2DmaTransport_Suspend();
            DiscardAndReset();
            s_state = COMM_STATE_APPLY_NEW_UART;
            break;
        case COMM_STATE_APPLY_NEW_UART:
            if (StartUart2(&s_candidate) && CommitCandidate() &&
                (!s_uart3_enabled ||
                 ModbusRtuServer_Resume(&s_server3, &s_candidate)))
                s_state = COMM_STATE_RESTART_RX;
            else s_state = COMM_STATE_ROLLBACK;
            break;
        case COMM_STATE_RESTART_RX:
            s_apply_requested = false;
            s_state = (s_active.protocol_mode == PROTOCOL_MODE_MODBUS_RTU) ?
                COMM_STATE_RUNNING : COMM_STATE_DISABLED;
            break;
        case COMM_STATE_ROLLBACK:
            if (StartUart2(&s_rollback) &&
                (!s_uart3_enabled ||
                 ModbusRtuServer_Resume(&s_server3, &s_rollback)))
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

CommunicationManagerState CommunicationManager_GetState(void)
{
    return s_state;
}

const CommunicationConfig *CommunicationManager_GetActiveConfig(void)
{
    return &s_active;
}

const ModbusRtuFramer *CommunicationManager_GetFramer(uint8_t port)
{
    return (port == 0U) ? &s_framer2 :
        ((port == 1U) && s_uart3_enabled ? &s_framer3 : NULL);
}

const ModbusRtuServer *CommunicationManager_GetServer(uint8_t port)
{
    return (port == 0U) ? &s_server2 :
        ((port == 1U) && s_uart3_enabled ? &s_server3 : NULL);
}

bool CommunicationManager_IsUart3Enabled(void)
{
    return s_uart3_enabled;
}

uint32_t CommunicationManager_GetFirstServiceCount(uint8_t port)
{
    return (port == 0U) ? s_uart2_first_count : s_uart3_first_count;
}
