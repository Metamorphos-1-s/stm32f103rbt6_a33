#include "a13c_shadow_compensator.h"

#include <stdint.h>
#include <stdio.h>
#include <time.h>

/* Desktop-only throughput smoke. It is NOT an STM32 cycle measurement. */
int main(void)
{
    A13CCompensator c;
    clock_t started;
    clock_t elapsed;
    uint32_t i;
    const uint32_t count = 100000U;
    A13C_Init(&c);
    if (!A13C_SetMode(&c, A13C_MODE_STATIC)) return 1;
    started = clock();
    for (i = 0U; i < count; ++i) {
        uint32_t phase = i % 20000U;
        int64_t mass = (phase >= 10000U ? INT64_C(500000000) : 0) +
            (int64_t)(i / 100U) * 80 + (int64_t)(i % 7U) * 200;
        if (!A13C_Feed(&c, i + 1U, 1000U + i * 100U, mass,
                       true, false, false, false)) return 2;
    }
    elapsed = clock() - started;
    (void)printf("HOST_ONLY samples=%lu cpu_s=%.6f us_per_sample=%.3f "
                 "gates=%lu\n", (unsigned long)count,
                 (double)elapsed / CLOCKS_PER_SEC,
                 ((double)elapsed * 1000000.0) /
                     ((double)CLOCKS_PER_SEC * (double)count),
                 (unsigned long)A13C_GetSnapshot(&c)->gate_count);
    return 0;
}
