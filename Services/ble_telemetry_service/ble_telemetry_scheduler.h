#ifndef BLE_TELEMETRY_SCHEDULER_H
#define BLE_TELEMETRY_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#define BLE_TELEMETRY_FAST_PERIOD_MS 200U
#define BLE_TELEMETRY_SLOW_PERIOD_MS 1000U
#define BLE_TELEMETRY_CHECKWEIGH_PERIOD_MS 1000U

typedef struct
{
    uint32_t frames_generated;
    uint32_t frames_sent;
    uint32_t frames_dropped_queue_full;
    uint32_t frames_dropped_transport_not_ready;
    uint32_t bytes_sent;
    uint32_t encode_errors;
    uint32_t fast_frames_generated;
    uint32_t fast_frames_sent;
    uint32_t fast_frames_dropped;
    uint32_t slow_frames_generated;
    uint32_t slow_frames_sent;
    uint32_t slow_frames_dropped;
    uint32_t checkweigh_frames_generated;
    uint32_t checkweigh_frames_sent;
    uint32_t checkweigh_frames_dropped;
} BleTelemetryCounters;

typedef bool (*BleTelemetryBuildFrame)(uint8_t type, uint16_t sequence,
    uint32_t timestamp_ms, uint8_t *buffer, uint16_t capacity,
    uint16_t *length, void *context);
typedef bool (*BleTelemetryTryWrite)(const uint8_t *data, uint16_t length,
    void *context);

typedef struct
{
    uint32_t next_fast_ms;
    uint32_t next_slow_ms;
    uint32_t next_checkweigh_ms;
    uint16_t next_sequence;
    BleTelemetryCounters counters;
} BleTelemetryScheduler;

void BleTelemetryScheduler_Init(BleTelemetryScheduler *scheduler,
                                uint32_t now_ms);
void BleTelemetryScheduler_Process(BleTelemetryScheduler *scheduler,
    uint32_t now_ms, bool transport_ready, BleTelemetryBuildFrame builder,
    BleTelemetryTryWrite writer, void *context);
void BleTelemetryScheduler_GetCounters(const BleTelemetryScheduler *scheduler,
                                       BleTelemetryCounters *counters);

#endif
