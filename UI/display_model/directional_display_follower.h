#ifndef DIRECTIONAL_DISPLAY_FOLLOWER_H
#define DIRECTIONAL_DISPLAY_FOLLOWER_H

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    int32_t display_count;
    uint32_t last_sample_sequence;
    int16_t evidence;
    uint8_t flags;
    uint8_t source;
} DirectionalDisplayFollower;

typedef struct
{
    int32_t desired_count;
    int32_t baseline_count;
    uint32_t sample_sequence;
    uint8_t source;
    bool stable;
    bool active;
    bool valid;
} DirectionalDisplayFollowerInput;

typedef struct
{
    int32_t desired_count;
    int32_t display_count;
    int64_t delta_count;
    int32_t anchor_count;
    int16_t evidence;
    int8_t direction;
    uint8_t release_reason;
    bool locked;
    bool stable;
    bool active;
    bool large_step;
} DirectionalDisplayFollowerOutput;

typedef struct
{
    int32_t display_count;
    uint32_t last_sample_sequence;
    int16_t evidence;
    uint8_t source;
    bool initialized;
    bool locked;
    bool last_large_step;
} DirectionalDisplayFollowerDiagnostics;

void DirectionalDisplayFollower_Reset(DirectionalDisplayFollower *follower);
bool DirectionalDisplayFollower_Process(DirectionalDisplayFollower *follower,
    const DirectionalDisplayFollowerInput *input,
    DirectionalDisplayFollowerOutput *output);
bool DirectionalDisplayFollower_GetDiagnostics(
    const DirectionalDisplayFollower *follower,
    DirectionalDisplayFollowerDiagnostics *diagnostics);

#endif
