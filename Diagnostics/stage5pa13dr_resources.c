#include "stage5pa13dr_stats.h"
#include "stage5pa13d_stack_scan.h"
#include "bsp_time.h"
#include "cs1237.h"
#include "measurement_bridge.h"
#include "metrology_manager.h"
#include "fault_manager.h"
#include "stm32f1xx_hal.h"
#include <stddef.h>

extern uint8_t _ebss;
extern uint8_t _estack;
static uintptr_t StaticEnd(void) { return ((uintptr_t)&_ebss + 7U) & ~(uintptr_t)7U; }
bool A13DR_Init(void)
{
    uint32_t overhead = 0U;
    uint32_t i;
    if (!ConfigStore_ClaimA13DRStats()) return false;
    if (!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk))
        g_a13dr_stats.stack_offset_flags |= UINT16_C(0x2000);
    for (i=0U;i<32U;++i) {
        uint32_t start = BSP_TimeNowCycles();
        uint32_t elapsed = BSP_TimeNowCycles() - start;
        if (elapsed > overhead) overhead = elapsed;
    }
    if (overhead > UINT16_MAX) g_a13dr_stats.stack_offset_flags |= UINT16_C(0x2000);
    else g_a13dr_stats.overhead_cycles = (uint16_t)overhead;
    return true;
}
__attribute__((naked,noinline)) void A13DR_PaintStack(void)
{
    __asm volatile(
        "mrs r3, primask\n cpsid i\n mrs r0, control\n tst r0,#2\n bne 3f\n"
        "mrs r0,ipsr\n cmp r0,#0\n bne 3f\n mrs r0,msp\n"
        "ldr r12,=_estack\n cmp r0,r12\n bhi 3f\n subs r0,r0,#64\n"
        "ldr r1,=_ebss\n adds r1,r1,#7\n bic r1,r1,#7\n cmp r1,r0\n bhs 3f\n"
        "ldr r2,=g_a13dr_boot_scratch\n ldrh r12,[r2,#132]\n subs r0,r0,r1\n"
        "cmp r0,#4096\n bhs 3f\n orr r12,r12,r0\n orr r12,r12,#4096\n"
        "strh r12,[r2,#132]\n adds r0,r0,r1\n ldr r2,=0xA55A3CC3\n"
        "1: cmp r1,r0\n bhs 2f\n str r2,[r1],#4\n b 1b\n"
        "3: ldr r2,=g_a13dr_boot_scratch\n ldrh r12,[r2,#132]\n"
        "orr r12,r12,#16384\n strh r12,[r2,#132]\n"
        "2: msr primask,r3\n bx lr\n");
}
uint32_t A13DR_LoopBegin(void)
{
    uint32_t now = BSP_TimeNowCycles();
    if (A13DR_BeginWrite(&g_a13dr_stats)) {
        uint32_t elapsed = now - g_a13dr_stats.previous_loop_start;
        if (g_a13dr_stats.loop_count && elapsed > g_a13dr_stats.loop_max_interval)
            g_a13dr_stats.loop_max_interval = elapsed;
        if (g_a13dr_stats.loop_count == UINT32_MAX)
            g_a13dr_stats.stack_offset_flags |= UINT16_C(0x2000);
        else ++g_a13dr_stats.loop_count;
        g_a13dr_stats.previous_loop_start = now;
        A13DR_EndWrite(&g_a13dr_stats);
    }
    return now;
}
void A13DR_LoopEnd(uint32_t start)
{
    uintptr_t low = StaticEnd();
    uintptr_t cursor;
    uint32_t elapsed;
    if (!A13DR_BeginWrite(&g_a13dr_stats)) return;
    if (g_a13dr_stats.stack_offset_flags & UINT16_C(0x1000)) {
        cursor = A13D_ScanUntouched(low, low +
            (g_a13dr_stats.stack_offset_flags & UINT16_C(0x0fff)), A13DR_PATTERN);
        g_a13dr_stats.stack_offset_flags = (uint16_t)(
            (g_a13dr_stats.stack_offset_flags & UINT16_C(0xf000)) | (uint16_t)(cursor-low));
        if (cursor-low < 512U) g_a13dr_stats.stack_offset_flags |= UINT16_C(0x8000);
    }
    elapsed = BSP_TimeNowCycles() - start;
    if (elapsed > g_a13dr_stats.loop_max_cycles) g_a13dr_stats.loop_max_cycles = elapsed;
    A13DR_EndWrite(&g_a13dr_stats);
}
void A13DR_FeedEnd(uint32_t start,uint32_t sequence,uint32_t state,bool boosted)
{
    uint32_t elapsed = BSP_TimeNowCycles()-start;
    A13DR_Accumulate(&g_a13dr_stats,elapsed,sequence,A13DR_Path(state,boosted));
}
uint32_t A13DR_ReadMetric(uint16_t index)
{
    const A13CSnapshot *candidate = MetrologyManager_GetA13CSnapshot();
    const MassSnapshot *mass = MetrologyManager_GetMassSnapshot();
    uint32_t path = g_a13dr_stats.global_peak_sequence_path >> 28U;
    uint32_t i;
    uint32_t total = 0U;
    if (!ConfigStore_A13DRStatsClaimed()) return 0U;
    if (index>=32U && index<42U) return g_a13dr_stats.max_cycles[index-32U];
    if (index>=42U && index<52U) return g_a13dr_stats.calls[index-42U];
    if (index>=56U && index<66U) return g_a13dr_stats.peak_sequence[index-56U];
    switch(index) {
    case 0U:return A13DR_SIGNATURE;
    case 1U:case 55U:return g_a13dr_stats.generation;
    case 2U:return HAL_RCC_GetHCLKFreq();
    case 3U:return ((uint32_t)g_a13dr_stats.overhead_cycles<<8U) | (g_a13dr_stats.stack_offset_flags>>12U);
    case 4U:return g_a13dr_stats.first_sequence;
    case 5U:return g_a13dr_stats.last_sequence;
    case 6U:
        for(i=0U;i<A13DR_PATHS;++i) total+=g_a13dr_stats.calls[i];
        return total;
    case 7U:return path<A13DR_PATHS ? g_a13dr_stats.max_cycles[path] : 0U;
    case 8U:return g_a13dr_stats.global_peak_sequence_path & UINT32_C(0x0fffffff);
    case 9U:return path;
    case 10U:return g_a13dr_stats.loop_max_cycles;
    case 11U:return g_a13dr_stats.loop_max_interval;
    case 12U:return g_a13dr_stats.loop_count;
    case 13U:return (uint32_t)(StaticEnd()+(g_a13dr_stats.stack_offset_flags & UINT16_C(0x0fff)));
    case 14U:return (uint32_t)StaticEnd();
    case 15U:return (uint32_t)(uintptr_t)&_estack;
    case 16U:return __get_CONTROL();
    case 17U:return BSP_TimeNowMs();
    case 18U:return CS1237_GetSampleCount();
    case 19U:return MeasurementBridge_GetConsumedCount();
    case 20U:return MeasurementBridge_GetInvalidCount();
    case 21U:return CS1237_GetBufferedSampleCount();
    case 22U:return CS1237_GetReadErrorCount();
    case 23U:return CS1237_GetBufferOverrunCount();
    case 24U:return mass!=NULL ? mass->sample_sequence : 0U;
    case 25U:return mass!=NULL ? mass->sample_timestamp_ms : 0U;
    case 26U:return candidate!=NULL ? (uint32_t)candidate->mode : 0U;
    case 27U:return candidate!=NULL ? (uint32_t)candidate->state : 0U;
    case 28U:return candidate!=NULL ? candidate->gate_count : 0U;
    case 29U:return candidate!=NULL ? candidate->boost_samples : 0U;
    case 30U:
        for(i=0U;i<A13DR_PATHS;++i) if(g_a13dr_stats.calls[i]) total|=1UL<<i;
        return total;
    case 31U:return 1U;
    case 52U:return FaultManager_GetActiveMask();
    case 53U:return 1U; /* Boot-scratch ownership valid. */
    case 54U:return CS1237_GetState();
    default:return 0U;
    }
}
_Static_assert(offsetof(A13DRStats,stack_offset_flags)==132U,"paint ABI");
_Static_assert(offsetof(A13DRStats,overhead_cycles)==134U,"overhead ABI");
