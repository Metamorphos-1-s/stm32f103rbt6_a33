#include "ble_telemetry_service.h"

#include "app_main.h"
#include "ble_checkweigh_codec.h"
#include "app_state.h"
#include "ble_frame_codec.h"
#include "ble_transport.h"
#include "fault_manager.h"
#include "metrology_manager.h"
#include "runtime_state.h"
#include "system_context.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

typedef struct
{
    int32_t raw_count;
    int32_t filtered_raw;
    int64_t display_mass_ug;
    int64_t operational_net_mass_ug;
    int64_t operational_gross_mass_ug;
    int64_t tare_mass_ug;
    int64_t uncompensated_gross_mass_ug;
    int64_t compensated_gross_mass_ug;
    int64_t runtime_drift_offset_ug;
    int64_t capacity_ug;
    int64_t overload_threshold_ug;
    uint32_t measurement_sequence;
    uint32_t status_flags;
    uint32_t timestamp_ms;
    uint32_t fault_mask;
    uint8_t runtime_drift_enabled;
    uint8_t runtime_drift_state;
    uint8_t persistent_dirty;
    uint8_t stable;
    uint8_t display_locked;
    uint8_t overload;
    uint8_t unit;
    uint8_t decimal_places;
    uint8_t division;
    uint8_t filter_mode;
    uint8_t filter_strength;
    uint8_t active_profile;
    uint8_t app_state;
    CheckweighState checkweigh_state;
    uint8_t weight_source;
    uint8_t limit_enabled;
    uint8_t alarm_active;
    uint8_t green_active;
    uint8_t yellow_active;
    uint8_t red_active;
    uint8_t internal_buzzer_active;
    uint8_t external_buzzer_active;
    uint32_t config_revision;
} BleTelemetrySnapshot;

static BleTelemetryScheduler s_scheduler;
static BleTelemetrySnapshot s_snapshot;
static bool s_initialized;

static bool CaptureSnapshot(uint32_t now_ms)
{
    const WeightSnapshot *weight = MetrologyManager_GetSnapshot();
    const DisplayConditionSnapshot *display =
        MetrologyManager_GetDisplayConditionSnapshot();
    const RuntimeDriftSnapshot *drift = MetrologyManager_GetRuntimeDriftSnapshot();
    const SystemContext *context = SystemContext_Get();
    const UnitDisplayConfig *unit_display;
    const WeighingProfileConfig *profile;
    AlarmOutputDiagnostics alarm = {0};
    bool have_alarm;
    if ((weight == NULL) || (display == NULL) || (context == NULL)) return false;
    unit_display = &context->config.metrology.unit_display[
        context->config.metrology.active_unit];
    profile = &context->config.metrology.profiles[
        context->config.metrology.active_profile];
    (void)memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_snapshot.raw_count = weight->raw_value;
    s_snapshot.filtered_raw = weight->filtered_raw;
    s_snapshot.display_mass_ug = display->display_mass_ug;
    s_snapshot.operational_net_mass_ug = weight->net_mass_ug;
    s_snapshot.operational_gross_mass_ug = weight->gross_mass_ug;
    s_snapshot.tare_mass_ug = weight->tare_mass_ug;
    s_snapshot.uncompensated_gross_mass_ug = weight->uncompensated_gross_mass_ug;
    s_snapshot.compensated_gross_mass_ug = weight->gross_mass_ug;
    s_snapshot.runtime_drift_offset_ug = (drift != NULL) ? drift->offset_ug : 0;
    s_snapshot.capacity_ug = context->config.metrology.capacity_ug;
    s_snapshot.overload_threshold_ug = context->config.metrology.overload_threshold_ug;
    s_snapshot.measurement_sequence = weight->sample_sequence;
    s_snapshot.status_flags = weight->status_flags;
    s_snapshot.timestamp_ms = (weight->sample_timestamp_ms != 0U) ?
        weight->sample_timestamp_ms : now_ms;
    s_snapshot.fault_mask = FaultManager_GetActiveMask();
    s_snapshot.runtime_drift_enabled = (drift != NULL && drift->enabled) ? 1U : 0U;
    s_snapshot.runtime_drift_state = (drift != NULL) ? (uint8_t)drift->state : 0U;
    s_snapshot.persistent_dirty = context->runtime.config_dirty ? 1U : 0U;
    s_snapshot.stable = (weight->status_flags & WEIGHT_STATUS_STABLE) != 0U;
    s_snapshot.display_locked = display->locked ? 1U : 0U;
    s_snapshot.overload = (weight->status_flags & WEIGHT_STATUS_OVERLOAD) != 0U;
    s_snapshot.unit = (uint8_t)context->config.metrology.active_unit;
    s_snapshot.decimal_places = unit_display->decimal_places;
    s_snapshot.division = unit_display->division_digit;
    s_snapshot.filter_mode = (uint8_t)profile->filter_mode;
    s_snapshot.filter_strength = profile->filter_strength;
    s_snapshot.active_profile = (uint8_t)context->config.metrology.active_profile;
    s_snapshot.app_state = (uint8_t)context->state;
    have_alarm = App_GetAlarmOutputDiagnostics(&alarm);
    s_snapshot.checkweigh_state = have_alarm ? alarm.checkweigh_state :
                                              CHECKWEIGH_DISABLED;
    s_snapshot.weight_source = (uint8_t)context->config.alarm.weight_source;
    s_snapshot.limit_enabled = context->config.alarm.limit_function_enable ? 1U : 0U;
    s_snapshot.alarm_active = (have_alarm &&
        (alarm.buzzer_mode == ALARM_BUZZER_MODE_ALARM)) ? 1U : 0U;
    s_snapshot.green_active = (have_alarm && alarm.green_active) ? 1U : 0U;
    s_snapshot.yellow_active = (have_alarm && alarm.yellow_active) ? 1U : 0U;
    s_snapshot.red_active = (have_alarm && alarm.red_active) ? 1U : 0U;
    s_snapshot.internal_buzzer_active =
        (have_alarm && alarm.internal_buzzer_active) ? 1U : 0U;
    s_snapshot.external_buzzer_active =
        (have_alarm && alarm.external_buzzer_active) ? 1U : 0U;
    s_snapshot.config_revision = context->config_revision;
    return true;
}

static bool BuildFrame(uint8_t type, uint16_t sequence, uint32_t timestamp_ms,
                       uint8_t *buffer, uint16_t capacity, uint16_t *length,
                       void *context)
{
    uint8_t payload[BLE_SLOW_PAYLOAD_SIZE];
    uint16_t offset = 0U;
    BleTelemetrySnapshot *snapshot = (BleTelemetrySnapshot *)context;
    if ((snapshot == NULL) || (buffer == NULL) || (length == NULL)) return false;
    if (type == BLE_MESSAGE_FAST_WEIGHT)
    {
        BleFrameCodec_PutU32(payload, &offset, snapshot->measurement_sequence);
        BleFrameCodec_PutI64(payload, &offset, snapshot->display_mass_ug);
        BleFrameCodec_PutI64(payload, &offset, snapshot->operational_net_mass_ug);
        BleFrameCodec_PutI64(payload, &offset, snapshot->operational_gross_mass_ug);
        BleFrameCodec_PutI64(payload, &offset, snapshot->tare_mass_ug);
        BleFrameCodec_PutU8(payload, &offset, snapshot->stable);
        BleFrameCodec_PutU8(payload, &offset, snapshot->display_locked);
        BleFrameCodec_PutU8(payload, &offset, snapshot->overload);
        BleFrameCodec_PutU8(payload, &offset, snapshot->unit);
        BleFrameCodec_PutU8(payload, &offset, snapshot->decimal_places);
        BleFrameCodec_PutU8(payload, &offset, snapshot->division);
    }
    else if (type == BLE_MESSAGE_SLOW_STATUS)
    {
        BleFrameCodec_PutI32(payload, &offset, snapshot->raw_count);
        BleFrameCodec_PutI32(payload, &offset, snapshot->filtered_raw);
        BleFrameCodec_PutI64(payload, &offset, snapshot->uncompensated_gross_mass_ug);
        BleFrameCodec_PutI64(payload, &offset, snapshot->compensated_gross_mass_ug);
        BleFrameCodec_PutI64(payload, &offset, snapshot->runtime_drift_offset_ug);
        BleFrameCodec_PutU8(payload, &offset, snapshot->runtime_drift_enabled);
        BleFrameCodec_PutU8(payload, &offset, snapshot->runtime_drift_state);
        BleFrameCodec_PutU8(payload, &offset, snapshot->persistent_dirty);
        BleFrameCodec_PutI64(payload, &offset, snapshot->capacity_ug);
        BleFrameCodec_PutI64(payload, &offset, snapshot->overload_threshold_ug);
        BleFrameCodec_PutU8(payload, &offset, snapshot->filter_mode);
        BleFrameCodec_PutU8(payload, &offset, snapshot->filter_strength);
        BleFrameCodec_PutU8(payload, &offset, snapshot->active_profile);
        BleFrameCodec_PutU8(payload, &offset, snapshot->app_state);
        BleFrameCodec_PutU32(payload, &offset, snapshot->fault_mask);
    }
    else if (type == BLE_MESSAGE_CHECKWEIGH_STATUS)
    {
        BleCheckweighStatusFields fields={0};
        fields.state=snapshot->checkweigh_state;
        fields.limit_enabled=snapshot->limit_enabled!=0U;
        fields.stable=snapshot->stable!=0U;
        fields.alarm_active=snapshot->alarm_active!=0U;
        fields.green_active=snapshot->green_active!=0U;
        fields.yellow_active=snapshot->yellow_active!=0U;
        fields.red_active=snapshot->red_active!=0U;
        fields.internal_buzzer_active=snapshot->internal_buzzer_active!=0U;
        fields.external_buzzer_active=snapshot->external_buzzer_active!=0U;
        fields.weight_source=(AlarmWeightSource)snapshot->weight_source;
        fields.config_revision=snapshot->config_revision;
        if(!BleCheckweighCodec_Encode(&fields,payload,sizeof(payload),&offset))
            return false;
    }
    else return false;
    return BleFrameCodec_Encode(type, sequence, timestamp_ms, payload, offset,
                                buffer, capacity, length);
}

static bool TryWrite(const uint8_t *data, uint16_t length, void *context)
{
    (void)context;
    return BleTransport_Write(data, length);
}

void BleTelemetryService_Init(uint32_t now_ms)
{
    BleTelemetryScheduler_Init(&s_scheduler, now_ms);
    (void)memset(&s_snapshot, 0, sizeof(s_snapshot));
    s_initialized = true;
}

void BleTelemetryService_Process(uint32_t now_ms)
{
    if (!s_initialized || !CaptureSnapshot(now_ms)) return;
    BleTelemetryScheduler_Process(&s_scheduler, now_ms, BleTransport_IsReady(),
                                  BuildFrame, TryWrite, &s_snapshot);
}

void BleTelemetryService_GetCounters(BleTelemetryCounters *counters)
{
    BleTelemetryScheduler_GetCounters(&s_scheduler, counters);
}
