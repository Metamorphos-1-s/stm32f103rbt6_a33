#include "stage5pa13d_resources.h"
#include "stage5pa13d_stack_scan.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define CHECK(x) do { if (!(x)) { (void)fprintf(stderr, "line %d: %s\n", __LINE__, #x); return 1; } } while (0)
_Static_assert(sizeof(A13DResourceState) == 24U, "RAM budget");
_Static_assert(offsetof(A13DResourceState, stack_offset_flags) == 20U, "paint ABI");
_Static_assert(offsetof(A13DResourceState, overhead_cycles) == 22U, "overhead ABI");

int main(void)
{
    uint32_t memory[256];
    uintptr_t low = (uintptr_t)&memory[0];
    uintptr_t limit = (uintptr_t)&memory[224];
    uintptr_t scanned;
    size_t index;
    volatile uint32_t cycle_start = UINT32_C(0xfffffff0);
    volatile uint32_t cycle_end = 32U;
    uint16_t packed;
    for (index = 0U; index < 256U; ++index) memory[index] = A13D_STACK_PATTERN;
    memory[224] = UINT32_C(0xFACE1234); /* excluded current-frame guard */
    scanned = A13D_ScanUntouched(low, limit, A13D_STACK_PATTERN);
    CHECK(scanned == limit);
    CHECK(memory[224] == UINT32_C(0xFACE1234));
    memory[180] = 0U;
    scanned = A13D_ScanUntouched(low, scanned, A13D_STACK_PATTERN);
    CHECK(scanned == (uintptr_t)&memory[180]);
    CHECK(scanned - low == 720U);
    memory[128] = 0U;
    scanned = A13D_ScanUntouched(low, scanned, A13D_STACK_PATTERN);
    CHECK(scanned - low == 512U);
    memory[127] = 0U;
    scanned = A13D_ScanUntouched(low, scanned, A13D_STACK_PATTERN);
    CHECK(scanned - low == 508U);
    packed = (uint16_t)(UINT16_C(0x1000) | (uint16_t)(scanned - low));
    CHECK((packed & UINT16_C(0x0fff)) == 508U);
    CHECK((packed >> 12U) == 1U);
    packed |= UINT16_C(0x8000);
    CHECK((packed >> 12U) == 9U);
    CHECK((uint32_t)(cycle_end - cycle_start) == 48U);
    (void)puts("A13D bounded scan/guard/ABI/DWT-wrap host checks PASS");
    return 0;
}
