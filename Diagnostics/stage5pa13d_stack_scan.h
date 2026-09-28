#ifndef STAGE5PA13D_STACK_SCAN_H
#define STAGE5PA13D_STACK_SCAN_H
#include <stdint.h>

/* Read-only, bounded scan; shared verbatim by target and host safety tests. */
static inline uintptr_t A13D_ScanUntouched(uintptr_t first,
                                         uintptr_t previous_lowest,
                                         uint32_t pattern)
{
    uintptr_t cursor = first;
    while (cursor < previous_lowest &&
           *(const volatile uint32_t *)cursor == pattern)
        cursor += sizeof(uint32_t);
    return cursor;
}
#endif
