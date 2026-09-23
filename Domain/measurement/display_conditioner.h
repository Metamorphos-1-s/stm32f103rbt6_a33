#ifndef DISPLAY_CONDITIONER_H
#define DISPLAY_CONDITIONER_H

#include "project_config.h"
#include "mass_types.h"
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
#include "unit_types.h"
#endif

#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_CONDITIONER_WINDOW_SIZE 9U
#define DISPLAY_CONDITIONER_RELEASE_SAMPLES 3U
#define DISPLAY_CONDITIONER_DEFAULT_HOLD_MS 1000U
#define DISPLAY_CONDITIONER_OPERATOR_UNSTABLE_TIMEOUT_MS 3000U

typedef enum
{
    DISPLAY_CONDITION_TRACKING = 0,
    DISPLAY_CONDITION_CANDIDATE,
    DISPLAY_CONDITION_LOCKED
} DisplayConditionState;

typedef enum
{
    DISPLAY_RELEASE_NONE = 0,
    DISPLAY_RELEASE_UNSTABLE,
    DISPLAY_RELEASE_DEVIATION,
    DISPLAY_RELEASE_OVERLOAD,
    DISPLAY_RELEASE_CALIBRATION,
    DISPLAY_RELEASE_NOT_ALLOWED,
    DISPLAY_RELEASE_FORCED,
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    DISPLAY_RELEASE_SOURCE_CHANGE,
    DISPLAY_RELEASE_LARGE_STEP,
    DISPLAY_RELEASE_SLOW_FOLLOW,
    DISPLAY_RELEASE_INVALID_DOMAIN
#endif
} DisplayConditionReleaseReason;

typedef struct
{
    MassValueUg authoritative_mass_ug;
    uint32_t now_ms;
    MassValueUg display_division_ug;
    uint32_t hold_ms;
    MassValueUg capacity_ug;
    bool stable;
    bool overload;
    bool calibrating;
    bool allow_lock;
    bool force_reset;
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    uint32_t sample_sequence;
    uint16_t source;
    MassUnit unit;
    uint8_t decimal_places;
    uint8_t division_digit;
#endif
} DisplayConditionInput;

typedef struct
{
    DisplayConditionState state;
    MassValueUg display_mass_ug;
    MassValueUg anchor_mass_ug;
    MassValueUg release_threshold_ug;
    uint32_t candidate_elapsed_ms;
    DisplayConditionReleaseReason last_release_reason;
    bool locked;
    bool operator_zero_anchor;
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    int32_t desired_display_count;
    int32_t display_count;
    uint32_t last_sample_sequence;
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
    uint32_t last_evidence_ms;
#endif
    int8_t direction;
    int8_t evidence;
    uint16_t source;
    bool display_domain_valid;
    bool large_step;
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
    bool have_evidence_time;
#endif
#endif
} DisplayConditionSnapshot;

typedef struct
{
    DisplayConditionSnapshot snapshot;
#if (A33_ENABLE_STAGE5MR5E_D1D_BETA != 0U)
    int32_t sample_buffer[DISPLAY_CONDITIONER_WINDOW_SIZE];
#else
    MassValueUg sample_buffer[DISPLAY_CONDITIONER_WINDOW_SIZE];
#endif
    uint32_t candidate_start_ms;
    uint32_t operator_anchor_start_ms;
    uint32_t last_update_ms;
    uint8_t sample_count;
    uint8_t sample_index;
    uint8_t release_sample_count;
    bool initialized;
} DisplayConditioner;

void DisplayConditioner_Init(DisplayConditioner *conditioner,
    MassValueUg initial_mass_ug, uint32_t now_ms);
void DisplayConditioner_ForceTracking(DisplayConditioner *conditioner,
    MassValueUg current_mass_ug, uint32_t now_ms,
    DisplayConditionReleaseReason reason);
bool DisplayConditioner_RequestOperatorZeroAnchor(
    DisplayConditioner *conditioner, uint32_t now_ms);
bool DisplayConditioner_Update(DisplayConditioner *conditioner,
    const DisplayConditionInput *input);
const DisplayConditionSnapshot *DisplayConditioner_GetSnapshot(
    const DisplayConditioner *conditioner);
MassValueUg DisplayConditioner_ComputeReleaseThreshold(
    MassValueUg display_division_ug, MassValueUg capacity_ug);

#endif /* DISPLAY_CONDITIONER_H */
