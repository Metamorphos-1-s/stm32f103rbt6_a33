#include "stage5pa13d_resources.h"
#include "stage5pa13d_stack_scan.h"
#include "bsp_time.h"
#include "cs1237.h"
#include "measurement_bridge.h"
#include "metrology_manager.h"
#include "stm32f1xx_hal.h"
#include <stddef.h>

A13DResourceState g_a13d_resources;
extern uint8_t _ebss;
extern uint8_t _estack;

static uintptr_t StaticEnd(void)
{
    return ((uintptr_t)&_ebss + 7U) & ~(uintptr_t)7U;
}

void A13D_Init(void)
{
    uint32_t index;
    uint32_t overhead_max = 0U;
    /* BSP's DWT already drives ADC edge timing. Do not reset its counter. */
    if ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) == 0U)
        g_a13d_resources.stack_offset_flags |= UINT16_C(0x2000);
    for (index = 0U; index < 32U; ++index) {
        uint32_t start = BSP_TimeNowCycles();
        uint32_t elapsed = BSP_TimeNowCycles() - start;
        if (elapsed > overhead_max) overhead_max = elapsed;
    }
    if (overhead_max > UINT16_MAX)
        g_a13d_resources.stack_offset_flags |= UINT16_C(0x2000);
    else
        g_a13d_resources.overhead_cycles = (uint16_t)overhead_max;
}

__attribute__((naked, noinline)) void A13D_PaintStack(void)
{
    __asm volatile(
        "mrs r3, primask\n"
        "cpsid i\n"
        "mrs r0, control\n"
        "tst r0, #2\n"
        "bne 3f\n"
        "mrs r0, ipsr\n"
        "cmp r0, #0\n"
        "bne 3f\n"
        "mrs r0, msp\n"
        "ldr r12, =_estack\n"
        "cmp r0, r12\n"
        "bhi 3f\n"
        "subs r0, r0, #64\n"
        "ldr r1, =_ebss\n"
        "adds r1, r1, #7\n"
        "bic r1, r1, #7\n"
        "cmp r1, r0\n"
        "bhs 3f\n"
        "ldr r2, =g_a13d_resources\n"
        "ldrh r12, [r2, #20]\n"
        "subs r0, r0, r1\n"
        "cmp r0, #4096\n"
        "bhs 3f\n"
        "orr r12, r12, r0\n"
        "orr r12, r12, #4096\n"
        "strh r12, [r2, #20]\n"
        "adds r0, r0, r1\n"
        "ldr r2, =0xA55A3CC3\n"
        "1: cmp r1, r0\n"
        "bhs 2f\n"
        "str r2, [r1], #4\n"
        "b 1b\n"
        "3: ldr r2, =g_a13d_resources\n"
        "ldrh r12, [r2, #20]\n"
        "orr r12, r12, #16384\n"
        "strh r12, [r2, #20]\n"
        "2: msr primask, r3\n"
        "bx lr\n");
}

uint32_t A13D_LoopBegin(void)
{
    uint32_t now = BSP_TimeNowCycles();
    uint32_t prior = g_a13d_resources.previous_loop_start;
    if (prior != 0U && now - prior > g_a13d_resources.loop_max_interval_cycles)
        g_a13d_resources.loop_max_interval_cycles = now - prior;
    g_a13d_resources.previous_loop_start = now;
    return now;
}

void A13D_LoopEnd(uint32_t start)
{
    uintptr_t cursor = StaticEnd();
    uint32_t elapsed;
    if ((g_a13d_resources.stack_offset_flags & UINT16_C(0x1000)) != 0U) {
        uintptr_t prior = StaticEnd() +
            (g_a13d_resources.stack_offset_flags & UINT16_C(0x0fff));
        cursor = A13D_ScanUntouched(cursor,
            prior, A13D_STACK_PATTERN);
        if (cursor < prior)
            g_a13d_resources.stack_offset_flags = (uint16_t)(
                (g_a13d_resources.stack_offset_flags & UINT16_C(0xf000)) |
                (uint16_t)(cursor - StaticEnd()));
        if (cursor - StaticEnd() < 512U)
            g_a13d_resources.stack_offset_flags |= UINT16_C(0x8000);
    }
    elapsed = BSP_TimeNowCycles() - start;
    if (elapsed > g_a13d_resources.loop_max_cycles)
        g_a13d_resources.loop_max_cycles = elapsed;
}

void A13D_FeedEnd(uint32_t start, uint32_t sequence, uint32_t state)
{
    uint32_t elapsed = BSP_TimeNowCycles() - start;
    if (sequence >= UINT32_C(0x10000000) || state >= 16U)
        g_a13d_resources.stack_offset_flags |= UINT16_C(0x2000);
    g_a13d_resources.feed_last_cycles = elapsed;
    /* Lossless in this bounded, reset-started diagnostic run. Refuse overflow;
     * never associate a prior call's cycles with a new mode-set snapshot. */
    g_a13d_resources.feed_last_sequence = sequence | (state << 28U);
}

uint32_t A13D_ReadMetric(uint16_t index)
{
    const A13CSnapshot *candidate = MetrologyManager_GetA13CSnapshot();
    const MassSnapshot *mass = MetrologyManager_GetMassSnapshot();
    switch (index) {
    case 0U: return HAL_RCC_GetHCLKFreq();
    case 1U: case 6U: return 0U;
    case 2U: return g_a13d_resources.feed_last_cycles;
    case 3U: return g_a13d_resources.feed_last_sequence & UINT32_C(0x0fffffff);
    case 5U: return g_a13d_resources.feed_last_sequence >> 28U;
    case 4U: return 0U; /* Exact maximum/sequence/state from complete host samples. */
    case 7U: return g_a13d_resources.loop_max_cycles;
    case 8U: return g_a13d_resources.loop_max_interval_cycles;
    case 9U: return g_a13d_resources.previous_loop_start;
    case 10U: return (uint32_t)(StaticEnd() +
        (g_a13d_resources.stack_offset_flags & UINT16_C(0x0fff)));
    case 11U: return ((uint32_t)g_a13d_resources.overhead_cycles << 8U) |
        (g_a13d_resources.stack_offset_flags >> 12U);
    case 12U: return (uint32_t)StaticEnd();
    case 13U: return (uint32_t)(uintptr_t)&_estack;
    case 14U: return __get_CONTROL();
    case 15U: return CS1237_GetSampleCount();
    case 16U: return MeasurementBridge_GetConsumedCount();
    case 17U: return MeasurementBridge_GetInvalidCount();
    case 18U: return CS1237_GetBufferedSampleCount();
    case 19U: return CS1237_GetReadErrorCount();
    case 20U: return CS1237_GetBufferOverrunCount();
    case 21U: return candidate != NULL ? candidate->sample_sequence : 0U;
    case 22U: return mass != NULL ? mass->sample_timestamp_ms : 0U;
    case 23U: return mass != NULL ? mass->sample_sequence : 0U;
    default: return 0U;
    }
}

_Static_assert(sizeof(A13DResourceState) == 24U, "A13D RAM budget");
_Static_assert(offsetof(A13DResourceState, stack_offset_flags) == 20U,
               "assembly watermark field offset");
_Static_assert(offsetof(A13DResourceState, overhead_cycles) == 22U,
               "assembly flag field offset");
