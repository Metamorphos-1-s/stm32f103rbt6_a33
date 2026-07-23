#ifndef TM1628_H
#define TM1628_H

#include "tm1628_types.h"
#include "project_config.h"

#include <stdbool.h>
#include <stdint.h>

bool TM1628_Init(uint8_t brightness);
void TM1628_Clear(void);
bool TM1628_SetBrightness(uint8_t brightness);
bool TM1628_SetDisplayEnabled(bool enabled);
bool TM1628_SetGridSegments(uint8_t grid_index, uint16_t segment_mask);
bool TM1628_Flush(void);
bool TM1628_IsDirty(void);
bool TM1628_ReadKeys(TM1628_KeyRaw *keys);
uint32_t TM1628_GetErrorCount(void);
void TM1628_ReleaseBus(void);

bool TM1628_EncodeGridSegments(uint8_t grid_index, uint16_t segment_mask,
                               uint8_t display_ram[TM1628_RAM_SIZE]);
uint8_t TM1628_DecodeBoardKeys(const uint8_t bytes[5]);

#endif /* TM1628_H */
