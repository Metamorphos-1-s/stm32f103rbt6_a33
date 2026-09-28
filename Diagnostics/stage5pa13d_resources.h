#ifndef STAGE5PA13D_RESOURCES_H
#define STAGE5PA13D_RESOURCES_H
#include <stdint.h>

#define A13D_STACK_PATTERN UINT32_C(0xA55A3CC3)
typedef struct {
    uint32_t feed_last_cycles;
    uint32_t feed_last_sequence;
    uint32_t loop_max_cycles;
    uint32_t loop_max_interval_cycles;
    uint32_t previous_loop_start;
    /* Exact byte offset (12 bits) plus four safety flags. No rounding. */
    uint16_t stack_offset_flags;
    uint16_t overhead_cycles;
} A13DResourceState;
extern A13DResourceState g_a13d_resources;
void A13D_Init(void);
void A13D_PaintStack(void);
uint32_t A13D_LoopBegin(void);
void A13D_LoopEnd(uint32_t start);
void A13D_FeedEnd(uint32_t start, uint32_t sequence, uint32_t state);
uint32_t A13D_ReadMetric(uint16_t index);
#endif
