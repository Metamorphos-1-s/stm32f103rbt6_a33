#ifndef MODBUS_RTU_FRAMER_H
#define MODBUS_RTU_FRAMER_H

#include "modbus_rtu_timing.h"

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
    uint16_t current_frame_length;
} ModbusRtuFramerStatistics;

bool ModbusRtuFramer_Init(const ModbusRtuTiming *timing);
void ModbusRtuFramer_Process(void);
void ModbusRtuFramer_OnIdleEvent(uint16_t dma_position,
                                uint32_t timestamp_cycles);
void ModbusRtuFramer_OnTimerEvent(void);
void ModbusRtuFramer_OnByte(uint8_t byte);
bool ModbusRtuFramer_TryGetFrame(uint8_t *destination, uint16_t capacity,
                                uint16_t *length);
void ModbusRtuFramer_Reset(void);
ModbusFramerState ModbusRtuFramer_GetState(void);
const ModbusRtuFramerStatistics *ModbusRtuFramer_GetStatistics(void);

#endif
