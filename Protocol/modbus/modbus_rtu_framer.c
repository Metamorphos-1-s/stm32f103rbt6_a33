#include "modbus_rtu_framer.h"

#include "bsp_rtu_timer.h"
#include "uart2_dma_transport.h"

#include <stddef.h>
#include <string.h>

#define FRAMER_MAX_BYTES_PER_PROCESS 128U

static ModbusRtuTiming s_timing;
static ModbusFramerState s_state;
static uint8_t s_frame[MODBUS_RTU_ADU_MAX_SIZE];
static uint16_t s_length;
static uint16_t s_silence_dma_position;
static ModbusRtuFramerStatistics s_statistics;

static void DiscardUntilSilence(void)
{
    s_length = 0U;
    s_statistics.current_frame_length = 0U;
    s_state = MODBUS_FRAMER_DISCARD_UNTIL_SILENCE;
    (void)BSP_RtuTimerStartUs(s_timing.t3_5_us);
}

static uint32_t RemainingAfterIdle(uint32_t interval_us)
{
    return (interval_us > s_timing.character_time_us) ?
        (interval_us - s_timing.character_time_us) : 1U;
}

bool ModbusRtuFramer_Init(const ModbusRtuTiming *timing)
{
    if ((timing == NULL) || (timing->t1_5_us == 0U) ||
        (timing->t3_5_us <= timing->t1_5_us)) return false;
    s_timing = *timing;
    (void)memset(&s_statistics, 0, sizeof(s_statistics));
    ModbusRtuFramer_Reset();
    DiscardUntilSilence();
    return true;
}

void ModbusRtuFramer_OnByte(uint8_t byte)
{
    if (s_state == MODBUS_FRAMER_FRAME_READY)
    {
        ++s_statistics.inter_character_error_count;
        DiscardUntilSilence();
        return;
    }
    if (s_state == MODBUS_FRAMER_WAIT_T3_5)
    {
        ++s_statistics.inter_character_error_count;
        DiscardUntilSilence();
        return;
    }
    if (s_state == MODBUS_FRAMER_DISCARD_UNTIL_SILENCE)
    {
        (void)BSP_RtuTimerStartUs(s_timing.t3_5_us);
        return;
    }
    if (s_state == MODBUS_FRAMER_WAIT_T1_5) BSP_RtuTimerStop();
    if (s_length >= MODBUS_RTU_ADU_MAX_SIZE)
    {
        ++s_statistics.overflow_count;
        DiscardUntilSilence();
        return;
    }
    s_frame[s_length++] = byte;
    s_statistics.current_frame_length = s_length;
    s_state = MODBUS_FRAMER_RECEIVING;
}

void ModbusRtuFramer_Process(void)
{
    uint16_t count = 0U;
    uint16_t position;
    uint32_t timestamp;
    uint8_t byte;
    if (Uart2DmaTransport_TakeReceiveError())
    {
        ++s_statistics.transport_error_count;
        Uart2DmaTransport_DiscardPending();
        DiscardUntilSilence();
    }
    while ((count < FRAMER_MAX_BYTES_PER_PROCESS) &&
           Uart2DmaTransport_TryReadByte(&byte))
    {
        ModbusRtuFramer_OnByte(byte);
        ++count;
    }
    if (Uart2DmaTransport_TakeIdleEvent(&position, &timestamp))
        ModbusRtuFramer_OnIdleEvent(position, timestamp);
    if (BSP_RtuTimerTakeElapsed()) ModbusRtuFramer_OnTimerEvent();
}

void ModbusRtuFramer_OnIdleEvent(uint16_t dma_position,
                                uint32_t timestamp_cycles)
{
    (void)timestamp_cycles;
    s_silence_dma_position = dma_position;
    if (s_state == MODBUS_FRAMER_RECEIVING)
    {
        s_state = MODBUS_FRAMER_WAIT_T1_5;
        (void)BSP_RtuTimerStartUs(RemainingAfterIdle(s_timing.t1_5_us));
    }
    else if (s_state == MODBUS_FRAMER_DISCARD_UNTIL_SILENCE)
    {
        (void)BSP_RtuTimerStartUs(RemainingAfterIdle(s_timing.t3_5_us));
    }
}

void ModbusRtuFramer_OnTimerEvent(void)
{
    uint16_t current = Uart2DmaTransport_GetStatistics()->dma_write_position;
    if (current != s_silence_dma_position)
    {
        if (s_state == MODBUS_FRAMER_WAIT_T3_5)
        {
            ++s_statistics.inter_character_error_count;
            DiscardUntilSilence();
        }
        else if (s_state == MODBUS_FRAMER_WAIT_T1_5)
        {
            s_state = MODBUS_FRAMER_RECEIVING;
        }
        else if (s_state == MODBUS_FRAMER_DISCARD_UNTIL_SILENCE)
        {
            (void)BSP_RtuTimerStartUs(s_timing.t3_5_us);
        }
        return;
    }
    if (s_state == MODBUS_FRAMER_WAIT_T1_5)
    {
        s_state = MODBUS_FRAMER_WAIT_T3_5;
        (void)BSP_RtuTimerStartUs(s_timing.t3_5_us - s_timing.t1_5_us);
    }
    else if (s_state == MODBUS_FRAMER_WAIT_T3_5)
    {
        if (s_length < 4U)
        {
            ++s_statistics.short_frame_count;
            ModbusRtuFramer_Reset();
        }
        else
        {
            s_state = MODBUS_FRAMER_FRAME_READY;
            ++s_statistics.frame_count;
        }
    }
    else if (s_state == MODBUS_FRAMER_DISCARD_UNTIL_SILENCE)
    {
        ModbusRtuFramer_Reset();
    }
}

bool ModbusRtuFramer_TryGetFrame(uint8_t *destination, uint16_t capacity,
                                uint16_t *length)
{
    if ((destination == NULL) || (length == NULL) ||
        (s_state != MODBUS_FRAMER_FRAME_READY) || (capacity < s_length))
        return false;
    (void)memcpy(destination, s_frame, s_length);
    *length = s_length;
    ModbusRtuFramer_Reset();
    return true;
}

void ModbusRtuFramer_Reset(void)
{
    BSP_RtuTimerStop();
    s_length = 0U;
    s_statistics.current_frame_length = 0U;
    s_state = MODBUS_FRAMER_WAITING;
    s_silence_dma_position =
        Uart2DmaTransport_GetStatistics()->dma_write_position;
}

ModbusFramerState ModbusRtuFramer_GetState(void) { return s_state; }
const ModbusRtuFramerStatistics *ModbusRtuFramer_GetStatistics(void)
{
    return &s_statistics;
}
