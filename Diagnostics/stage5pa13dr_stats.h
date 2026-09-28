#ifndef STAGE5PA13DR_STATS_H
#define STAGE5PA13DR_STATS_H
#include "persistent_schema.h"
#include <stdbool.h>
#include <stdint.h>

#define A13DR_PATHS 10U
#define A13DR_SIGNATURE UINT32_C(0xA13D5201)
#define A13DR_PATTERN UINT32_C(0xA55A3CC3)
typedef struct {
    uint32_t max_cycles[A13DR_PATHS];
    uint16_t calls[A13DR_PATHS];
    uint32_t peak_sequence[A13DR_PATHS];
    uint32_t global_peak_sequence_path;
    uint32_t first_sequence;
    uint32_t last_sequence;
    uint32_t loop_max_cycles;
    uint32_t loop_max_interval;
    uint32_t previous_loop_start;
    uint32_t generation;
    uint32_t loop_count;
    uint16_t stack_offset_flags;
    uint16_t overhead_cycles;
} A13DRStats;
typedef union {
    uint8_t boot_payload[CONFIG_STORE_PAYLOAD_BUFFER_SIZE];
    A13DRStats stats;
} A13DRBootScratch;
extern A13DRBootScratch g_a13dr_boot_scratch;
#define g_a13dr_stats (g_a13dr_boot_scratch.stats)
bool ConfigStore_ClaimA13DRStats(void);
bool ConfigStore_A13DRStatsClaimed(void);
bool A13DR_Init(void);
void A13DR_PaintStack(void);
uint32_t A13DR_LoopBegin(void);
void A13DR_LoopEnd(uint32_t start);
void A13DR_FeedEnd(uint32_t start, uint32_t sequence, uint32_t state, bool boosted);
uint32_t A13DR_ReadMetric(uint16_t index);

/* Shared exact updater for MCU and host tests. All callers are main-loop only. */
static inline bool A13DR_BeginWrite(A13DRStats *s)
{
    if ((s->generation & 1U) || s->generation >= UINT32_MAX - 1U) {
        s->stack_offset_flags |= UINT16_C(0x2000);
        return false;
    }
    ++s->generation;
    return true;
}
static inline void A13DR_EndWrite(A13DRStats *s) { ++s->generation; }
static inline uint32_t A13DR_Path(uint32_t state, bool boosted)
{
    switch (state) {
    case 0U:return 0U; case 4U:return 1U; case 5U:return 2U;
    case 6U:return 3U; case 7U:return boosted ? 5U : 4U;
    case 3U:return 6U; case 2U:return 7U; case 1U:return 8U;
    default:return 9U;
    }
}
static inline void A13DR_Accumulate(A13DRStats *s, uint32_t cycles,
                                    uint32_t sequence, uint32_t path)
{
    uint32_t winner = s->global_peak_sequence_path >> 28U;
    uint32_t prior_max = s->max_cycles[winner < A13DR_PATHS ? winner : 9U];
    if (!A13DR_BeginWrite(s)) return;
    if (path >= A13DR_PATHS) path = 9U;
    if (sequence == 0U || sequence >= UINT32_C(0x10000000) || path == 9U ||
        s->calls[path] == UINT16_MAX)
        s->stack_offset_flags |= UINT16_C(0x2000);
    if (s->first_sequence == 0U) s->first_sequence = sequence;
    else if (sequence != s->last_sequence + 1U)
        s->stack_offset_flags |= UINT16_C(0x2000);
    s->last_sequence = sequence;
    if (s->calls[path] != UINT16_MAX) ++s->calls[path];
    if (cycles > s->max_cycles[path]) {
        s->max_cycles[path] = cycles;
        s->peak_sequence[path] = sequence;
    }
    if (cycles > prior_max)
        s->global_peak_sequence_path = (sequence & UINT32_C(0x0fffffff)) | (path << 28U);
    A13DR_EndWrite(s);
}
_Static_assert(sizeof(A13DRStats) == 136U, "statistics ABI");
_Static_assert(sizeof(A13DRStats) <= CONFIG_STORE_PAYLOAD_BUFFER_SIZE, "boot scratch fit");
#endif
