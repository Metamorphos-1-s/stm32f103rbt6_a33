#include "directional_display_follower.h"

#include <stddef.h>
#include <string.h>

#define FLAG_INITIALIZED       0x01U
#define FLAG_LOCKED            0x02U
#define FLAG_LARGE_STEP        0x04U
#define EVIDENCE_INCREMENT     1
#define EVIDENCE_ZERO_LEAK     1
#define EVIDENCE_REVERSE_CANCEL 1
#define EVIDENCE_THRESHOLD     5
#define LARGE_STEP_DIVISIONS   8U

static int8_t Direction(int64_t value)
{
    return (value > 0) ? 1 : ((value < 0) ? -1 : 0);
}

static uint64_t Magnitude(int64_t value)
{
    return (value < 0) ? (uint64_t)(-(value + 1)) + 1U : (uint64_t)value;
}

static void Publish(const DirectionalDisplayFollower *follower,
    const DirectionalDisplayFollowerInput *input, bool large_step,
    DirectionalDisplayFollowerOutput *output)
{
    int64_t delta = (int64_t)input->desired_count - follower->display_count;
    output->desired_count = input->desired_count;
    output->display_count = follower->display_count;
    output->delta_count = delta;
    output->anchor_count = follower->display_count;
    output->evidence = follower->evidence;
    output->direction = Direction(delta);
    output->release_reason = large_step ? 2U : 0U;
    output->locked = (follower->flags & FLAG_LOCKED) != 0U;
    output->stable = input->stable;
    output->active = input->active;
    output->large_step = large_step;
}

void DirectionalDisplayFollower_Reset(DirectionalDisplayFollower *follower)
{
    if (follower != NULL)
        (void)memset(follower, 0, sizeof(*follower));
}

static void Accumulate(DirectionalDisplayFollower *follower, int8_t direction)
{
    if (direction == 0)
    {
        if (follower->evidence > 0)
            follower->evidence -= EVIDENCE_ZERO_LEAK;
        else if (follower->evidence < 0)
            follower->evidence += EVIDENCE_ZERO_LEAK;
    }
    else if ((follower->evidence == 0) ||
             (Direction(follower->evidence) == direction))
    {
        int16_t next = (int16_t)(follower->evidence +
            direction * EVIDENCE_INCREMENT);
        int16_t limit = EVIDENCE_THRESHOLD + EVIDENCE_INCREMENT;
        follower->evidence = (next > limit) ? limit :
            ((next < -limit) ? (int16_t)-limit : next);
    }
    else
    {
        int16_t magnitude = (follower->evidence < 0) ?
            (int16_t)-follower->evidence : follower->evidence;
        int16_t cancel = (magnitude < EVIDENCE_REVERSE_CANCEL) ?
            magnitude : EVIDENCE_REVERSE_CANCEL;
        follower->evidence = (int16_t)(follower->evidence +
            direction * cancel);
    }
}

bool DirectionalDisplayFollower_Process(DirectionalDisplayFollower *follower,
    const DirectionalDisplayFollowerInput *input,
    DirectionalDisplayFollowerOutput *output)
{
    int64_t delta;
    int8_t direction;
    if ((follower == NULL) || (input == NULL) || (output == NULL))
        return false;
    if (((follower->flags & FLAG_INITIALIZED) == 0U) || !input->valid ||
        (follower->source != input->source))
    {
        DirectionalDisplayFollower_Reset(follower);
        follower->display_count = input->baseline_count;
        follower->last_sample_sequence = input->sample_sequence;
        follower->source = input->source;
        follower->flags = input->valid ?
            (FLAG_INITIALIZED | FLAG_LOCKED) : 0U;
        Publish(follower, input, false, output);
        return true;
    }
    if (!input->active)
    {
        follower->display_count = input->baseline_count;
        follower->last_sample_sequence = input->sample_sequence;
        follower->evidence = 0;
        follower->flags = FLAG_INITIALIZED;
        Publish(follower, input, false, output);
        return true;
    }
    delta = (int64_t)input->desired_count - follower->display_count;
    if (Magnitude(delta) > LARGE_STEP_DIVISIONS)
    {
        follower->display_count = input->desired_count;
        follower->evidence = 0;
        follower->last_sample_sequence = input->sample_sequence;
        follower->flags = FLAG_INITIALIZED | FLAG_LARGE_STEP;
        Publish(follower, input, true, output);
        return true;
    }
    follower->flags = (uint8_t)((follower->flags | FLAG_LOCKED) &
        (uint8_t)~(uint8_t)FLAG_LARGE_STEP);
    direction = Direction(delta);
    if (input->sample_sequence != follower->last_sample_sequence)
    {
        follower->last_sample_sequence = input->sample_sequence;
        if (input->stable)
            Accumulate(follower, direction);
        else if (direction == 0)
            Accumulate(follower, 0);
    }
    if ((direction != 0) && (Direction(follower->evidence) == direction) &&
        (((follower->evidence < 0) ? -follower->evidence :
            follower->evidence) >= EVIDENCE_THRESHOLD))
    {
        follower->display_count += direction;
        follower->evidence = 0;
    }
    Publish(follower, input, false, output);
    return true;
}

bool DirectionalDisplayFollower_GetDiagnostics(
    const DirectionalDisplayFollower *follower,
    DirectionalDisplayFollowerDiagnostics *diagnostics)
{
    if ((follower == NULL) || (diagnostics == NULL))
        return false;
    diagnostics->display_count = follower->display_count;
    diagnostics->last_sample_sequence = follower->last_sample_sequence;
    diagnostics->evidence = follower->evidence;
    diagnostics->source = follower->source;
    diagnostics->initialized =
        (follower->flags & FLAG_INITIALIZED) != 0U;
    diagnostics->locked = (follower->flags & FLAG_LOCKED) != 0U;
    diagnostics->last_large_step =
        (follower->flags & FLAG_LARGE_STEP) != 0U;
    return true;
}
