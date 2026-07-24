#ifndef UART2_DMA_POSITION_H
#define UART2_DMA_POSITION_H

#include <stdbool.h>
#include <stdint.h>

bool Uart2DmaPosition_Resolve(uint32_t previous_absolute,
                             uint32_t completed_wraps,
                             uint16_t position,
                             uint16_t buffer_size,
                             uint32_t *resolved_absolute,
                             bool *wrap_race_compensated);

#endif
