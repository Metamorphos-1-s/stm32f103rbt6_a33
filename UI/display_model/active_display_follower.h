#ifndef ACTIVE_DISPLAY_FOLLOWER_H
#define ACTIVE_DISPLAY_FOLLOWER_H
#include <stdbool.h>
#include <stdint.h>
typedef struct { int32_t display_count; int32_t candidate_count;
    uint32_t candidate_start_ms; uint16_t confirmation_count;
    uint8_t flags; uint8_t source; } ActiveDisplayFollower;
typedef struct { int32_t desired_count; int32_t baseline_count; uint32_t now_ms;
    uint8_t source; bool stable; bool active; bool valid; } ActiveDisplayFollowerInput;
typedef struct { int32_t desired_count; int32_t display_count; int32_t anchor_count;
    uint16_t confirmation_count; uint8_t release_reason; bool locked;
    bool large_step; } ActiveDisplayFollowerOutput;
void ActiveDisplayFollower_Reset(ActiveDisplayFollower *follower);
bool ActiveDisplayFollower_Process(ActiveDisplayFollower *follower,
    const ActiveDisplayFollowerInput *input, ActiveDisplayFollowerOutput *output);
#endif
