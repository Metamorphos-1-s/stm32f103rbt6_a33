#ifndef A13C_SHADOW_COMPENSATOR_H
#define A13C_SHADOW_COMPENSATOR_H

#include <stdbool.h>
#include <stdint.h>

#define A13C_REFERENCE_SAMPLES 300U
#define A13C_OBSERVATION_SAMPLES 200U
#define A13C_HISTORY_CAPACITY A13C_REFERENCE_SAMPLES
#define A13C_HISTORY_BYTES (A13C_HISTORY_CAPACITY * 3U)
#define A13C_STEP_SAMPLES 60U

typedef enum {
    A13C_MODE_OFF = 0,
    A13C_MODE_DOSING = 1,
    A13C_MODE_STATIC = 2
} A13CMode;

typedef enum {
    A13C_STATE_OFF = 0,
    A13C_STATE_DOSING,
    A13C_STATE_STEP_SETTLING,
    A13C_STATE_STEP_PENDING,
    A13C_STATE_HOLDOFF,
    A13C_STATE_REFERENCE_FILL,
    A13C_STATE_OBSERVATION_FILL,
    A13C_STATE_TRACKING,
    A13C_STATE_LIMITED
} A13CState;

typedef enum {
    A13C_REASON_NONE = 0,
    A13C_REASON_INITIAL,
    A13C_REASON_TIME_GAP,
    A13C_REASON_STATIC_STEP_PENDING,
    A13C_REASON_STATIC_STEP_BASELINE_ONLY,
    A13C_REASON_STATIC_STEP_FAST,
    A13C_REASON_STATIC_STEP_RETURNED,
    A13C_REASON_STATIC_STEP_UNSETTLED,
    A13C_REASON_DOSING_ENTRY,
    A13C_REASON_DOSING_EXIT,
    A13C_REASON_DOSING_RECENT_STEP,
    A13C_REASON_ZERO,
    A13C_REASON_CALIBRATION,
    A13C_REASON_PROFILE,
    A13C_REASON_INVALID_INPUT,
    A13C_REASON_FAULT,
    A13C_REASON_OVERLOAD,
    A13C_REASON_NEAR_RAIL,
    A13C_REASON_REPRESENTATION,
    A13C_REASON_NUMERIC
} A13CReason;

typedef struct {
    int64_t uncompensated_ug;
    int64_t corrected_ug;
    int64_t offset_ug;
    int64_t reference_twice_ug;
    uint32_t sample_sequence;
    uint32_t timestamp_ms;
    uint32_t gate_count;
    uint32_t rebuild_count;
    uint32_t boost_samples;
    uint32_t obvious_step_sequence;
    uint32_t robust_step_sequence;
    uint32_t quiet_sequence;
    uint32_t reference_lock_sequence;
    uint32_t first_correction_sequence;
    A13CMode mode;
    A13CState state;
    A13CReason reason;
    bool limited;
} A13CSnapshot;

typedef struct {
    /* One lossless signed-24-bit history: reference and observation never
     * occupy it simultaneously after reference median is cached. */
    uint8_t history[A13C_HISTORY_BYTES];
    /* The current 3 kg engineering envelope maps exactly into unsigned 32
     * bits with a -0.5 kg anchor; out-of-range shadow input freezes safely. */
    uint32_t step_encoded[A13C_STEP_SAMPLES];
    int64_t history_base_ug;
    int64_t reference_twice_ug;
    int64_t event_reference_twice_ug;
    int64_t offset_ug;
    int64_t last_mass_ug;
    int64_t pre_step_mass_ug;
    uint32_t last_sequence;
    uint32_t last_timestamp_ms;
    uint32_t hold_until;
    uint32_t freeze_until;
    uint32_t boost_until;
    uint32_t event_sequence;
    uint32_t obvious_sequence;
    uint32_t pending_sequence;
    uint16_t history_count;
    uint16_t history_head;
    uint16_t reference_count;
    uint16_t observation_count;
    uint16_t event_reference_count;
    uint16_t event_observation_count;
    uint8_t step_head;
    uint8_t step_fill;
    uint8_t step_count;
    int8_t step_sign;
    bool step_armed;
    bool have_last;
    bool reference_valid;
    bool event_reference_valid;
    bool event_pending;
    bool obvious_valid;
    bool have_pre_step_mass;
    bool limited;
    bool was_dosing;
    A13CMode mode;
    A13CState state;
    A13CReason reason;
    A13CSnapshot snapshot;
} A13CCompensator;

void A13C_Init(A13CCompensator *controller);
bool A13C_SetMode(A13CCompensator *controller, A13CMode mode);
void A13C_Reset(A13CCompensator *controller, A13CReason reason);
bool A13C_Feed(A13CCompensator *controller, uint32_t sequence,
               uint32_t timestamp_ms, int64_t gross_ug, bool valid,
               bool fault, bool overload, bool near_rail);
const A13CSnapshot *A13C_GetSnapshot(const A13CCompensator *controller);

#endif
