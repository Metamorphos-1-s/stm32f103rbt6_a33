#ifndef MODBUS_RTU_FRAMER_H
#define MODBUS_RTU_FRAMER_H

#include "modbus_rtu_timing.h"
#include "modbus_rtu_transport.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    MODBUS_FRAMER_WAITING = 0,
    MODBUS_FRAMER_RECEIVING,
    MODBUS_FRAMER_WAIT_T1_5,
    MODBUS_FRAMER_WAIT_T3_5,
    MODBUS_FRAMER_FRAME_READY,
    MODBUS_FRAMER_DISCARD_UNTIL_SILENCE,
    MODBUS_FRAMER_ERROR
} ModbusFramerState;

typedef struct
{
    uint32_t frame_count;
    uint32_t short_frame_count;
    uint32_t inter_character_error_count;
    uint32_t overflow_count;
    uint32_t transport_error_count;
    uint32_t idle_event_count;
    uint32_t timer_event_count;
    uint32_t timer_race_count;
    uint32_t timer_t1_5_elapsed_count;
    uint32_t timer_t3_5_elapsed_count;
    uint32_t timer_start_failure_count;
    uint16_t current_frame_length;
} ModbusRtuFramerStatistics;

#define MODBUS_RTU_ADU_MAX_SIZE 256U
#define MODBUS_RTU_MAX_BYTES_PER_PROCESS 128U

typedef struct ModbusRtuFramer
{
    ModbusRtuTiming timing;
    ModbusFramerState state;
    uint8_t frame[MODBUS_RTU_ADU_MAX_SIZE];
    uint16_t length;
    uint16_t silence_position;
    uint32_t timer_start_cycles;
    uint32_t timer_delay_cycles;
    bool timer_active;
    ModbusRtuFramerStatistics statistics;
} ModbusRtuFramer;

bool ModbusRtuFramer_Init(ModbusRtuFramer *framer,
                          const ModbusRtuTiming *timing,
                          uint16_t initial_position);
void ModbusRtuFramer_Process(ModbusRtuFramer *framer,
                             const ModbusRtuTransport *transport);
void ModbusRtuFramer_OnIdleEvent(ModbusRtuFramer *framer,
                                uint16_t dma_position,
                                uint32_t timestamp_cycles);
void ModbusRtuFramer_OnTimerEvent(ModbusRtuFramer *framer,
                                 uint16_t current_position);
void ModbusRtuFramer_OnByte(ModbusRtuFramer *framer, uint8_t byte);
bool ModbusRtuFramer_TryGetFrame(ModbusRtuFramer *framer,
                                uint8_t *destination, uint16_t capacity,
                                uint16_t *length);
void ModbusRtuFramer_Reset(ModbusRtuFramer *framer,
                           uint16_t current_position);
ModbusFramerState ModbusRtuFramer_GetState(const ModbusRtuFramer *framer);
const ModbusRtuFramerStatistics *ModbusRtuFramer_GetStatistics(
    const ModbusRtuFramer *framer);

#endif
