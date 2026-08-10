#include "ble_telemetry_scheduler.h"

#include "ble_frame_codec.h"

#include <stddef.h>
#include <string.h>

static bool Due(uint32_t now, uint32_t deadline)
{
    return (int32_t)(now - deadline) >= 0;
}

static void RunOne(BleTelemetryScheduler *scheduler, uint32_t now_ms,
                   uint8_t type, uint32_t *deadline, uint32_t period,
                   bool transport_ready, BleTelemetryBuildFrame builder,
                   BleTelemetryTryWrite writer, void *context)
{
    uint8_t frame[BLE_SLOW_FRAME_SIZE];
    uint16_t length = 0U;
    uint16_t sequence;
    bool is_fast = (type == BLE_MESSAGE_FAST_WEIGHT);
    bool is_slow = (type == BLE_MESSAGE_SLOW_STATUS);
    if (!Due(now_ms, *deadline)) return;
    *deadline += period;
    sequence = scheduler->next_sequence++;
    if ((builder == NULL) || !builder(type, sequence, now_ms, frame,
                                       (uint16_t)sizeof(frame), &length,
                                       context))
    {
        ++scheduler->counters.encode_errors;
        return;
    }
    ++scheduler->counters.frames_generated;
    if (is_fast) ++scheduler->counters.fast_frames_generated;
    else if (is_slow) ++scheduler->counters.slow_frames_generated;
    else ++scheduler->counters.checkweigh_frames_generated;
    if (!transport_ready)
    {
        ++scheduler->counters.frames_dropped_transport_not_ready;
        if (is_fast) ++scheduler->counters.fast_frames_dropped;
        else if (is_slow) ++scheduler->counters.slow_frames_dropped;
        else ++scheduler->counters.checkweigh_frames_dropped;
        return;
    }
    if ((writer != NULL) && writer(frame, length, context))
    {
        ++scheduler->counters.frames_sent;
        scheduler->counters.bytes_sent += length;
        if (is_fast) ++scheduler->counters.fast_frames_sent;
        else if (is_slow) ++scheduler->counters.slow_frames_sent;
        else ++scheduler->counters.checkweigh_frames_sent;
    }
    else
    {
        ++scheduler->counters.frames_dropped_queue_full;
        if (is_fast) ++scheduler->counters.fast_frames_dropped;
        else if (is_slow) ++scheduler->counters.slow_frames_dropped;
        else ++scheduler->counters.checkweigh_frames_dropped;
    }
}

void BleTelemetryScheduler_Init(BleTelemetryScheduler *scheduler,
                                uint32_t now_ms)
{
    if (scheduler == NULL) return;
    (void)memset(scheduler, 0, sizeof(*scheduler));
    scheduler->next_fast_ms = now_ms + BLE_TELEMETRY_FAST_PERIOD_MS;
    scheduler->next_slow_ms = now_ms + BLE_TELEMETRY_SLOW_PERIOD_MS;
    scheduler->next_checkweigh_ms = now_ms + BLE_TELEMETRY_CHECKWEIGH_PERIOD_MS;
}

void BleTelemetryScheduler_Process(BleTelemetryScheduler *scheduler,
    uint32_t now_ms, bool transport_ready, BleTelemetryBuildFrame builder,
    BleTelemetryTryWrite writer, void *context)
{
    if ((scheduler == NULL) || (builder == NULL)) return;
    RunOne(scheduler, now_ms, BLE_MESSAGE_FAST_WEIGHT,
           &scheduler->next_fast_ms, BLE_TELEMETRY_FAST_PERIOD_MS,
           transport_ready, builder, writer, context);
    RunOne(scheduler, now_ms, BLE_MESSAGE_SLOW_STATUS,
           &scheduler->next_slow_ms, BLE_TELEMETRY_SLOW_PERIOD_MS,
           transport_ready, builder, writer, context);
    RunOne(scheduler, now_ms, BLE_MESSAGE_CHECKWEIGH_STATUS,
           &scheduler->next_checkweigh_ms,
           BLE_TELEMETRY_CHECKWEIGH_PERIOD_MS,
           transport_ready, builder, writer, context);
    if (Due(now_ms, scheduler->next_fast_ms))
        scheduler->next_fast_ms = now_ms + BLE_TELEMETRY_FAST_PERIOD_MS;
    if (Due(now_ms, scheduler->next_slow_ms))
        scheduler->next_slow_ms = now_ms + BLE_TELEMETRY_SLOW_PERIOD_MS;
    if (Due(now_ms, scheduler->next_checkweigh_ms))
        scheduler->next_checkweigh_ms = now_ms +
            BLE_TELEMETRY_CHECKWEIGH_PERIOD_MS;
}

void BleTelemetryScheduler_GetCounters(const BleTelemetryScheduler *scheduler,
                                       BleTelemetryCounters *counters)
{
    if ((scheduler != NULL) && (counters != NULL)) *counters = scheduler->counters;
}
