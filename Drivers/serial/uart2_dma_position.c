#include "uart2_dma_position.h"

#include <stddef.h>

bool Uart2DmaPosition_Resolve(uint32_t previous_absolute,
                             uint32_t completed_wraps,
                             uint16_t position,
                             uint16_t buffer_size,
                             uint32_t *resolved_absolute,
                             bool *wrap_race_compensated)
{
    uint32_t candidate;
    if ((resolved_absolute == NULL) || (wrap_race_compensated == NULL) ||
        (buffer_size == 0U) || (position >= buffer_size))
    {
        return false;
    }
    candidate = completed_wraps * buffer_size + position;
    *wrap_race_compensated = false;
    /* Absolute positions are modulo 2^32; a delta over half-range is backward. */
    if ((candidate - previous_absolute) > (UINT32_MAX / 2U))
    {
        if ((candidate >= previous_absolute) ||
            ((previous_absolute - candidate) >= buffer_size))
        {
            return false;
        }
        candidate += buffer_size;
        *wrap_race_compensated = true;
    }
    *resolved_absolute = candidate;
    return true;
}
