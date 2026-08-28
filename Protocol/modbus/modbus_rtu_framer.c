#include "modbus_rtu_framer.h"

#include "bsp_time.h"

#include <stddef.h>
#include <string.h>

static bool IntervalElapsed(uint32_t timestamp_cycles, uint32_t interval_us)
{
    uint32_t interval_cycles;
    return BSP_TimeUsToCycles(interval_us, &interval_cycles) &&
        BSP_TimeCyclesElapsed(BSP_TimeNowCycles(), timestamp_cycles,
                              interval_cycles);
}

static void StopTimer(ModbusRtuFramer *framer)
{
    framer->timer_active = false;
}

static void StartTimer(ModbusRtuFramer *framer, uint32_t delay_us)
{
    uint32_t delay_cycles;
    if (!BSP_TimeUsToCycles(delay_us, &delay_cycles) || (delay_cycles == 0U))
    {
        framer->timer_active = false;
        ++framer->statistics.timer_start_failure_count;
        return;
    }
    framer->timer_start_cycles = BSP_TimeNowCycles();
    framer->timer_delay_cycles = delay_cycles;
    framer->timer_active = true;
}

static void CompleteReceivedFrame(ModbusRtuFramer *framer)
{
    StopTimer(framer);
    if (framer->length < 4U)
    {
        ++framer->statistics.short_frame_count;
        ModbusRtuFramer_Reset(framer, framer->silence_position);
    }
    else
    {
        framer->state = MODBUS_FRAMER_FRAME_READY;
        ++framer->statistics.frame_count;
    }
}

static void DiscardUntilSilence(ModbusRtuFramer *framer)
{
    framer->length = 0U;
    framer->statistics.current_frame_length = 0U;
    framer->state = MODBUS_FRAMER_DISCARD_UNTIL_SILENCE;
    StartTimer(framer, framer->timing.t3_5_us);
}

static uint32_t RemainingAfterIdle(const ModbusRtuFramer *framer,
                                   uint32_t interval_us)
{
    return (interval_us > framer->timing.character_time_us) ?
        (interval_us - framer->timing.character_time_us) : 1U;
}

bool ModbusRtuFramer_Init(ModbusRtuFramer *framer,
                          const ModbusRtuTiming *timing,
                          uint16_t initial_position)
{
    if ((framer == NULL) || (timing == NULL) || (timing->t1_5_us == 0U) ||
        (timing->t3_5_us <= timing->t1_5_us)) return false;
    (void)memset(framer, 0, sizeof(*framer));
    framer->timing = *timing;
    ModbusRtuFramer_Reset(framer, initial_position);
    DiscardUntilSilence(framer);
    return true;
}

void ModbusRtuFramer_OnByte(ModbusRtuFramer *framer, uint8_t byte)
{
    if (framer == NULL) return;
    if ((framer->state == MODBUS_FRAMER_FRAME_READY) ||
        (framer->state == MODBUS_FRAMER_WAIT_T3_5))
    {
        ++framer->statistics.inter_character_error_count;
        DiscardUntilSilence(framer);
        return;
    }
    if (framer->state == MODBUS_FRAMER_DISCARD_UNTIL_SILENCE)
    {
        StartTimer(framer, framer->timing.t3_5_us);
        return;
    }
    if (framer->state == MODBUS_FRAMER_WAIT_T1_5) StopTimer(framer);
    if (framer->length >= MODBUS_RTU_ADU_MAX_SIZE)
    {
        ++framer->statistics.overflow_count;
        DiscardUntilSilence(framer);
        return;
    }
    framer->frame[framer->length++] = byte;
    framer->statistics.current_frame_length = framer->length;
    framer->state = MODBUS_FRAMER_RECEIVING;
}

void ModbusRtuFramer_Process(ModbusRtuFramer *framer,
                             const ModbusRtuTransport *transport)
{
    uint16_t count = 0U;
    uint16_t position;
    uint32_t timestamp;
    uint8_t byte;
    if ((framer == NULL) || !ModbusRtuTransport_IsValid(transport)) return;
    if (transport->take_receive_error(transport->context))
    {
        ++framer->statistics.transport_error_count;
        transport->discard_pending(transport->context);
        DiscardUntilSilence(framer);
    }
    while ((count < MODBUS_RTU_MAX_BYTES_PER_PROCESS) &&
           transport->try_read_byte(transport->context, &byte))
    {
        ModbusRtuFramer_OnByte(framer, byte);
        ++count;
    }
    if (transport->take_idle_event(transport->context, &position, &timestamp))
        ModbusRtuFramer_OnIdleEvent(framer, position, timestamp);
    if (framer->timer_active && BSP_TimeCyclesElapsed(BSP_TimeNowCycles(),
        framer->timer_start_cycles, framer->timer_delay_cycles))
        ModbusRtuFramer_OnTimerEvent(framer,
            transport->get_rx_position(transport->context));
}

void ModbusRtuFramer_OnIdleEvent(ModbusRtuFramer *framer,
                                 uint16_t dma_position,
                                 uint32_t timestamp_cycles)
{
    if (framer == NULL) return;
    ++framer->statistics.idle_event_count;
    framer->silence_position = dma_position;
    if (framer->state == MODBUS_FRAMER_RECEIVING)
    {
        if (IntervalElapsed(timestamp_cycles,
            RemainingAfterIdle(framer, framer->timing.t3_5_us)))
        {
            CompleteReceivedFrame(framer);
            return;
        }
        framer->state = MODBUS_FRAMER_WAIT_T1_5;
        StartTimer(framer,
            RemainingAfterIdle(framer, framer->timing.t1_5_us));
    }
    else if (framer->state == MODBUS_FRAMER_DISCARD_UNTIL_SILENCE)
    {
        if (IntervalElapsed(timestamp_cycles,
            RemainingAfterIdle(framer, framer->timing.t3_5_us)))
        {
            ModbusRtuFramer_Reset(framer, dma_position);
            return;
        }
        StartTimer(framer,
            RemainingAfterIdle(framer, framer->timing.t3_5_us));
    }
}

void ModbusRtuFramer_OnTimerEvent(ModbusRtuFramer *framer,
                                  uint16_t current_position)
{
    if (framer == NULL) return;
    StopTimer(framer);
    ++framer->statistics.timer_event_count;
    if (current_position != framer->silence_position)
    {
        ++framer->statistics.timer_race_count;
        if (framer->state == MODBUS_FRAMER_WAIT_T3_5)
        {
            ++framer->statistics.inter_character_error_count;
            DiscardUntilSilence(framer);
        }
        else if (framer->state == MODBUS_FRAMER_WAIT_T1_5)
            framer->state = MODBUS_FRAMER_RECEIVING;
        else if (framer->state == MODBUS_FRAMER_DISCARD_UNTIL_SILENCE)
            StartTimer(framer, framer->timing.t3_5_us);
        return;
    }
    if (framer->state == MODBUS_FRAMER_WAIT_T1_5)
    {
        ++framer->statistics.timer_t1_5_elapsed_count;
        framer->state = MODBUS_FRAMER_WAIT_T3_5;
        StartTimer(framer,
            framer->timing.t3_5_us - framer->timing.t1_5_us);
    }
    else if (framer->state == MODBUS_FRAMER_WAIT_T3_5)
    {
        ++framer->statistics.timer_t3_5_elapsed_count;
        CompleteReceivedFrame(framer);
    }
    else if (framer->state == MODBUS_FRAMER_DISCARD_UNTIL_SILENCE)
        ModbusRtuFramer_Reset(framer, current_position);
}

bool ModbusRtuFramer_TryGetFrame(ModbusRtuFramer *framer,
                                 uint8_t *destination, uint16_t capacity,
                                 uint16_t *length)
{
    if ((framer == NULL) || (destination == NULL) || (length == NULL) ||
        (framer->state != MODBUS_FRAMER_FRAME_READY) ||
        (capacity < framer->length)) return false;
    (void)memcpy(destination, framer->frame, framer->length);
    *length = framer->length;
    ModbusRtuFramer_Reset(framer, framer->silence_position);
    return true;
}

void ModbusRtuFramer_Reset(ModbusRtuFramer *framer,
                           uint16_t current_position)
{
    if (framer == NULL) return;
    StopTimer(framer);
    framer->length = 0U;
    framer->statistics.current_frame_length = 0U;
    framer->state = MODBUS_FRAMER_WAITING;
    framer->silence_position = current_position;
}

ModbusFramerState ModbusRtuFramer_GetState(const ModbusRtuFramer *framer)
{
    return (framer != NULL) ? framer->state : MODBUS_FRAMER_ERROR;
}

const ModbusRtuFramerStatistics *ModbusRtuFramer_GetStatistics(
    const ModbusRtuFramer *framer)
{
    return (framer != NULL) ? &framer->statistics : NULL;
}
