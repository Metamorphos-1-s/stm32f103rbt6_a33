#define _CRT_SECURE_NO_WARNINGS 1
#include "a13c_shadow_compensator.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* Stdin rows: sequence,timestamp_ms,gross_ug,mode (1 DOSING/2 STATIC).
 * This is a HOST-ONLY parity runner, never linked into firmware. */
int main(void)
{
    A13CCompensator controller;
    char line[160];
    A13CMode last_mode = A13C_MODE_OFF;
    A13C_Init(&controller);
    while (fgets(line, sizeof(line), stdin) != NULL) {
        char *cursor = line;
        char *end;
        uint32_t seq, ms;
        int64_t mass;
        unsigned long mode_value;
        const A13CSnapshot *s;
        seq = (uint32_t)strtoul(cursor, &end, 10);
        if (end == cursor || *end != ',') return 2;
        cursor = end + 1;
        ms = (uint32_t)strtoul(cursor, &end, 10);
        if (end == cursor || *end != ',') return 2;
        cursor = end + 1;
        mass = (int64_t)strtoll(cursor, &end, 10);
        if (end == cursor || *end != ',') return 2;
        cursor = end + 1;
        mode_value = strtoul(cursor, &end, 10);
        if (end == cursor || (mode_value != 1UL && mode_value != 2UL)) return 2;
        if (last_mode != (A13CMode)mode_value) {
            if (!A13C_SetMode(&controller, (A13CMode)mode_value)) return 3;
            last_mode = (A13CMode)mode_value;
        }
        if (!A13C_Feed(&controller, seq, ms, mass, true,
                       false, false, false)) return 4;
        s = A13C_GetSnapshot(&controller);
        if (s == NULL) return 5;
        if (printf("%" PRIu32 ",%" PRId64 ",%" PRId64
                   ",%u,%" PRIu32 ",%" PRIu32 ",%" PRIu32
                   ",%u,%" PRIu32 ",%" PRIu32 ",%" PRIu32
                   ",%" PRIu32 ",%" PRIu32 ",%u\n",
                   s->sample_sequence, s->corrected_ug, s->offset_ug,
                   (unsigned)s->state, s->gate_count, s->rebuild_count,
                   s->boost_samples, (unsigned)s->reason,
                   s->obvious_step_sequence, s->robust_step_sequence,
                   s->quiet_sequence, s->reference_lock_sequence,
                   s->first_correction_sequence, s->limited ? 1U : 0U) < 0)
            return 6;
    }
    return ferror(stdin) ? 7 : 0;
}
