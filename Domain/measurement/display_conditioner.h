#ifndef DISPLAY_CONDITIONER_H
#define DISPLAY_CONDITIONER_H

#include "mass_types.h"

#include <stdbool.h>
#include <stdint.h>

#define DISPLAY_CONDITIONER_WINDOW_SIZE 9U
#define DISPLAY_CONDITIONER_RELEASE_SAMPLES 3U
#define DISPLAY_CONDITIONER_DEFAULT_HOLD_MS 1000U
#define DISPLAY_CONDITIONER_OPERATOR_GRACE_MS 1000U
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
    DISPLAY_RELEASE_FORCED
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
} DisplayConditionSnapshot;

typedef struct
{
    DisplayConditionSnapshot snapshot;
    MassValueUg sample_buffer[DISPLAY_CONDITIONER_WINDOW_SIZE];
    uint32_t candidate_start_ms;
    uint32_t operator_anchor_start_ms;
    MassValueUg operator_release_reference_ug;
    uint32_t last_update_ms;
    uint8_t sample_count;
    uint8_t sample_index;
    uint8_t release_sample_count;
    bool operator_reference_pending;
    bool initialized;
} DisplayConditioner;

void DisplayConditioner_Init(DisplayConditioner *conditioner,
    MassValueUg initial_mass_ug, uint32_t now_ms);
void DisplayConditioner_ForceTracking(DisplayConditioner *conditioner,
    MassValueUg current_mass_ug, uint32_t now_ms,
    DisplayConditionReleaseReason reason);
bool DisplayConditioner_RequestOperatorZeroAnchor(
    DisplayConditioner *conditioner, MassValueUg authoritative_mass_ug,
    uint32_t now_ms);
bool DisplayConditioner_Update(DisplayConditioner *conditioner,
    const DisplayConditionInput *input);
const DisplayConditionSnapshot *DisplayConditioner_GetSnapshot(
    const DisplayConditioner *conditioner);
MassValueUg DisplayConditioner_ComputeReleaseThreshold(
    MassValueUg display_division_ug, MassValueUg capacity_ug);

#endif /* DISPLAY_CONDITIONER_H */
