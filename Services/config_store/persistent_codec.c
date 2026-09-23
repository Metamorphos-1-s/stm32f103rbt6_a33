#include "persistent_codec.h"

#include "default_config.h"
#include "device_config_validator.h"
#include "persistent_schema.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

typedef struct { uint8_t *data; uint16_t capacity; uint16_t position; bool failed; } CodecWriter;
typedef struct { const uint8_t *data; uint16_t length; uint16_t position; bool failed; } CodecReader;

static void PutU8(CodecWriter *w, uint8_t v) { if (w->position >= w->capacity) { w->failed = true; return; } w->data[w->position++] = v; }
static void PutU16(CodecWriter *w, uint16_t v) { PutU8(w, (uint8_t)v); PutU8(w, (uint8_t)(v >> 8U)); }
static void PutU32(CodecWriter *w, uint32_t v) { PutU16(w, (uint16_t)v); PutU16(w, (uint16_t)(v >> 16U)); }
static void PutU64(CodecWriter *w, uint64_t v) { PutU32(w, (uint32_t)v); PutU32(w, (uint32_t)(v >> 32U)); }
static uint8_t GetU8(CodecReader *r) { if (r->position >= r->length) { r->failed = true; return 0U; } return r->data[r->position++]; }
static uint16_t GetU16(CodecReader *r) { uint16_t v = GetU8(r); v |= (uint16_t)((uint16_t)GetU8(r) << 8U); return v; }
static uint32_t GetU32(CodecReader *r) { uint32_t v = GetU16(r); v |= (uint32_t)GetU16(r) << 16U; return v; }
static uint64_t GetU64(CodecReader *r) { uint64_t v = GetU32(r); v |= (uint64_t)GetU32(r) << 32U; return v; }
static bool GetBool(CodecReader *r, bool *v) { uint8_t e = GetU8(r); if (r->failed || e > 1U) return false; *v = e != 0U; return true; }
static void PutI32(CodecWriter *w, int32_t v) { uint32_t b; (void)memcpy(&b, &v, sizeof(b)); PutU32(w, b); }
static void PutI64(CodecWriter *w, int64_t v) { uint64_t b; (void)memcpy(&b, &v, sizeof(b)); PutU64(w, b); }
static int32_t GetI32(CodecReader *r) { uint32_t b = GetU32(r); int32_t v; (void)memcpy(&v, &b, sizeof(v)); return v; }
static int64_t GetI64(CodecReader *r) { uint64_t b = GetU64(r); int64_t v; (void)memcpy(&v, &b, sizeof(v)); return v; }

bool PersistentCodec_ValidateConfig(const DeviceConfig *config) { return DeviceConfig_Validate(config); }

PersistentCodecResult PersistentCodec_EncodeV3(
    const DeviceConfig *config, const RuntimeState *runtime,
    uint8_t *buffer, uint16_t capacity, uint16_t *encoded_length)
{
    CodecWriter w = {buffer, capacity, 0U, false};
    uint8_t i;
    if ((config == NULL) || (runtime == NULL) || (buffer == NULL) || (encoded_length == NULL)) return PERSISTENT_CODEC_NULL;
    *encoded_length = 0U;
    if (capacity < PERSISTENT_V3_PAYLOAD_SIZE) return PERSISTENT_CODEC_BUFFER_TOO_SMALL;
    if (!PersistentCodec_ValidateConfig(config) || ((uint32_t)runtime->weight_view >= WEIGHT_VIEW_COUNT)) return PERSISTENT_CODEC_VALIDATION_FAILED;
    PutU64(&w, (uint64_t)config->metrology.capacity_ug); PutU64(&w, (uint64_t)config->metrology.verification_interval_e_ug);
    PutU64(&w, (uint64_t)config->metrology.zero_range_ug); PutU64(&w, (uint64_t)config->metrology.overload_threshold_ug);
    PutU64(&w, (uint64_t)config->metrology.auto_zero_tracking_range_ug);
    PutU16(&w, config->metrology.initial_zero_range_permille); PutU16(&w, config->metrology.semi_auto_zero_range_permille);
    PutU8(&w, (uint8_t)config->metrology.compliance_mode); PutU8(&w, (uint8_t)config->metrology.active_unit); PutU8(&w, config->metrology.enabled_unit_mask);
    for (i = 0U; i < MASS_UNIT_COUNT; ++i) { PutU8(&w, config->metrology.unit_display[i].enabled ? 1U : 0U); PutU8(&w, config->metrology.unit_display[i].decimal_places); PutU32(&w, config->metrology.unit_display[i].division_digit); }
    PutU8(&w, config->metrology.load_cell.rated_capacity_known ? 1U : 0U); PutU64(&w, (uint64_t)config->metrology.load_cell.rated_capacity_ug);
    PutU8(&w, config->metrology.load_cell.sensitivity_known ? 1U : 0U); PutU32(&w, config->metrology.load_cell.sensitivity_uv_per_v);
    PutU8(&w, config->metrology.load_cell.safe_load_known ? 1U : 0U); PutU16(&w, config->metrology.load_cell.safe_load_permille);
    for (i = 0U; i < WEIGHING_PROFILE_COUNT; ++i) { const WeighingProfileConfig *p = &config->metrology.profiles[i]; PutU8(&w, (uint8_t)p->sample_rate); PutU8(&w, (uint8_t)p->gain); PutU8(&w, (uint8_t)p->filter_mode); PutU8(&w, p->filter_strength); PutU8(&w, p->stability_window); PutU64(&w, (uint64_t)p->stability_enter_threshold_ug); PutU64(&w, (uint64_t)p->stability_exit_threshold_ug); PutU32(&w, p->stability_hold_ms); }
    PutU8(&w, (uint8_t)config->metrology.active_profile); PutI32(&w, config->calibration.raw_zero); PutI32(&w, config->calibration.raw_span); PutI32(&w, config->calibration.scale_numerator); PutI32(&w, config->calibration.scale_denominator); PutU32(&w, config->calibration.calibration_sequence); PutU8(&w, config->calibration.calibration_valid ? 1U : 0U); PutU64(&w, (uint64_t)config->calibration.span_mass_ug);
    PutU16(&w, config->stability.window_size); PutU32(&w, config->stability.enter_threshold); PutU32(&w, config->stability.exit_threshold); PutU32(&w, config->stability.stable_hold_ms);
    PutU32(&w, config->communication.baud_rate); PutU8(&w, (uint8_t)config->communication.parity); PutU8(&w, (uint8_t)config->communication.stop_bits); PutU8(&w, config->communication.modbus_address); PutU8(&w, (uint8_t)config->communication.protocol_mode); PutU8(&w, (uint8_t)config->communication.output_policy); PutU32(&w, config->communication.output_period_ms); PutU32(&w, config->communication.zero_suppress_range); PutU8(&w, (uint8_t)config->communication.word_order); PutU16(&w, config->communication.response_delay_ms); PutU16(&w, config->communication.recommended_poll_interval_ms); PutU8(&w, config->communication.broadcast_write_policy); PutU8(&w, config->communication.pending_apply ? 1U : 0U);
    PutU32(&w, config->bluetooth.uart_baud_rate); PutU16(&w, config->bluetooth.protocol_version); PutU8(&w, config->bluetooth.w02_configured ? 1U : 0U);
    PutI64(&w, config->alarm.lower_limit_ug); PutI64(&w, config->alarm.upper_limit_ug); PutI64(&w, config->alarm.hysteresis_ug); PutU8(&w, (uint8_t)config->alarm.weight_source); PutU8(&w, config->alarm.internal_buzzer_enable ? 1U : 0U); PutU8(&w, config->alarm.external_buzzer_enable ? 1U : 0U); PutU8(&w, config->alarm.qualified_beep_enable ? 1U : 0U); PutU8(&w, config->alarm.limit_function_enable ? 1U : 0U);
    PutU8(&w, config->display.brightness); PutU8(&w, config->display.default_weight_view); PutU32(&w, config->battery.divider_top_ohm); PutU32(&w, config->battery.divider_bottom_ohm); PutI32(&w, config->battery.calibration_gain_ppm); PutI32(&w, config->battery.calibration_offset_mv); PutU32(&w, config->battery.low_warning_mv); PutU32(&w, config->battery.critical_low_mv); PutU32(&w, config->battery.recovery_mv); PutU8(&w, config->battery.low_voltage_alarm_enable ? 1U : 0U); PutU8(&w, config->system.tare_power_loss_retention ? 1U : 0U); PutU8(&w, config->system.watchdog_enable ? 1U : 0U); PutU8(&w, config->system.startup_auto_zero_enable ? 1U : 0U);
    PutU8(&w, (uint8_t)runtime->weight_view); PutU64(&w, (uint64_t)runtime->current_tare_ug); PutU8(&w, runtime->tare_active ? 1U : 0U);
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
    PutU8(&w, (uint8_t)((config->system.requested_r5_mode & 0x03U) |
        ((config->system.requested_r5_application & 0x01U) << 2U) |
        ((config->system.requested_checkweigh_mode & 0x03U) << 4U)));
#else
    PutU8(&w, 0U);
#endif
    if (w.failed || w.position != PERSISTENT_V3_PAYLOAD_SIZE) return PERSISTENT_CODEC_BUFFER_TOO_SMALL;
    *encoded_length = w.position; return PERSISTENT_CODEC_OK;
}

PersistentCodecResult PersistentCodec_DecodeV3(const uint8_t *buffer, uint16_t length, DeviceConfig *config, RuntimeState *runtime)
{
    CodecReader r = {buffer, length, 0U, false}; uint8_t i; bool valid = true;
    if ((buffer == NULL) || (config == NULL) || (runtime == NULL)) return PERSISTENT_CODEC_NULL;
    if (length < PERSISTENT_V3_PAYLOAD_SIZE) return PERSISTENT_CODEC_TRUNCATED;
    if (length != PERSISTENT_V3_PAYLOAD_SIZE) return PERSISTENT_CODEC_INVALID_VALUE;
    DefaultConfig_Load(config); (void)memset(runtime, 0, sizeof(*runtime));
    config->metrology.capacity_ug=(MassValueUg)GetU64(&r); config->metrology.verification_interval_e_ug=(MassValueUg)GetU64(&r); config->metrology.zero_range_ug=(MassValueUg)GetU64(&r); config->metrology.overload_threshold_ug=(MassValueUg)GetU64(&r); config->metrology.auto_zero_tracking_range_ug=(MassValueUg)GetU64(&r); config->metrology.initial_zero_range_permille=GetU16(&r); config->metrology.semi_auto_zero_range_permille=GetU16(&r); config->metrology.compliance_mode=(MetrologyComplianceMode)GetU8(&r); config->metrology.active_unit=(MassUnit)GetU8(&r); config->metrology.enabled_unit_mask=GetU8(&r);
    for(i=0U;i<MASS_UNIT_COUNT;++i){valid&=GetBool(&r,&config->metrology.unit_display[i].enabled);config->metrology.unit_display[i].decimal_places=GetU8(&r);config->metrology.unit_display[i].division_digit=(uint8_t)GetU32(&r);} valid&=GetBool(&r,&config->metrology.load_cell.rated_capacity_known);config->metrology.load_cell.rated_capacity_ug=(MassValueUg)GetU64(&r);valid&=GetBool(&r,&config->metrology.load_cell.sensitivity_known);config->metrology.load_cell.sensitivity_uv_per_v=GetU32(&r);valid&=GetBool(&r,&config->metrology.load_cell.safe_load_known);config->metrology.load_cell.safe_load_permille=GetU16(&r);
    for(i=0U;i<WEIGHING_PROFILE_COUNT;++i){WeighingProfileConfig *p=&config->metrology.profiles[i];p->sample_rate=(Cs1237DataRate)GetU8(&r);p->gain=(Cs1237Gain)GetU8(&r);p->filter_mode=(FilterMode)GetU8(&r);p->filter_strength=GetU8(&r);p->stability_window=GetU8(&r);p->stability_enter_threshold_ug=(MassValueUg)GetU64(&r);p->stability_exit_threshold_ug=(MassValueUg)GetU64(&r);p->stability_hold_ms=GetU32(&r);} config->metrology.active_profile=(WeighingProfileId)GetU8(&r); config->calibration.raw_zero=GetI32(&r);config->calibration.raw_span=GetI32(&r);config->calibration.scale_numerator=GetI32(&r);config->calibration.scale_denominator=GetI32(&r);config->calibration.calibration_sequence=GetU32(&r);valid&=GetBool(&r,&config->calibration.calibration_valid);config->calibration.span_mass_ug=(MassValueUg)GetU64(&r);
    config->stability.window_size=GetU16(&r);config->stability.enter_threshold=GetU32(&r);config->stability.exit_threshold=GetU32(&r);config->stability.stable_hold_ms=GetU32(&r);config->communication.baud_rate=GetU32(&r);config->communication.parity=(CommunicationParity)GetU8(&r);config->communication.stop_bits=(CommunicationStopBits)GetU8(&r);config->communication.modbus_address=GetU8(&r);config->communication.protocol_mode=(ProtocolMode)GetU8(&r);config->communication.output_policy=(OutputPolicy)GetU8(&r);config->communication.output_period_ms=GetU32(&r);config->communication.zero_suppress_range=GetU32(&r);config->communication.word_order=(ModbusWordOrder)GetU8(&r);config->communication.response_delay_ms=GetU16(&r);config->communication.recommended_poll_interval_ms=GetU16(&r);config->communication.broadcast_write_policy=GetU8(&r);valid&=GetBool(&r,&config->communication.pending_apply);config->bluetooth.uart_baud_rate=GetU32(&r);config->bluetooth.protocol_version=GetU16(&r);valid&=GetBool(&r,&config->bluetooth.w02_configured);
    config->alarm.lower_limit_ug=GetI64(&r);config->alarm.upper_limit_ug=GetI64(&r);config->alarm.hysteresis_ug=GetI64(&r);config->alarm.weight_source=(AlarmWeightSource)GetU8(&r);valid&=GetBool(&r,&config->alarm.internal_buzzer_enable);valid&=GetBool(&r,&config->alarm.external_buzzer_enable);valid&=GetBool(&r,&config->alarm.qualified_beep_enable);valid&=GetBool(&r,&config->alarm.limit_function_enable);config->display.brightness=GetU8(&r);config->display.default_weight_view=GetU8(&r);config->battery.divider_top_ohm=GetU32(&r);config->battery.divider_bottom_ohm=GetU32(&r);config->battery.calibration_gain_ppm=GetI32(&r);config->battery.calibration_offset_mv=GetI32(&r);config->battery.low_warning_mv=GetU32(&r);config->battery.critical_low_mv=GetU32(&r);config->battery.recovery_mv=GetU32(&r);valid&=GetBool(&r,&config->battery.low_voltage_alarm_enable);valid&=GetBool(&r,&config->system.tare_power_loss_retention);valid&=GetBool(&r,&config->system.watchdog_enable);valid&=GetBool(&r,&config->system.startup_auto_zero_enable);runtime->weight_view=(WeightViewMode)GetU8(&r);runtime->current_tare_ug=(MassValueUg)GetU64(&r);runtime->current_tare=(runtime->current_tare_ug>INT32_MAX)?INT32_MAX:(int32_t)runtime->current_tare_ug;valid&=GetBool(&r,&runtime->tare_active);
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
    { uint8_t requested = GetU8(&r);
      config->system.requested_r5_mode = requested & 0x03U;
      config->system.requested_r5_application = (requested >> 2U) & 0x01U;
      config->system.requested_checkweigh_mode = (requested >> 4U) & 0x03U;
      if ((config->system.requested_r5_mode > 2U) ||
          (config->system.requested_checkweigh_mode > 2U) ||
          ((requested & 0xC8U) != 0U)) {
          config->system.requested_r5_mode = 0U;
          config->system.requested_r5_application = 0U;
          config->system.requested_checkweigh_mode = 0U;
      } }
#else
    valid&=(GetU8(&r)==0U);
#endif
    if (r.failed || !valid || (r.position != PERSISTENT_V3_PAYLOAD_SIZE) ||
        !PersistentCodec_ValidateConfig(config) ||
        ((uint32_t)runtime->weight_view >= WEIGHT_VIEW_COUNT))
    {
        return r.failed ? PERSISTENT_CODEC_TRUNCATED :
            PERSISTENT_CODEC_VALIDATION_FAILED;
    }
    return PERSISTENT_CODEC_OK;
}

#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
static bool PersistentCodec_MetrologyEqual(const MetrologyConfig *a,
                                           const MetrologyConfig *b)
{
    uint8_t i;
    if ((a->capacity_ug != b->capacity_ug) ||
        (a->verification_interval_e_ug != b->verification_interval_e_ug) ||
        (a->zero_range_ug != b->zero_range_ug) ||
        (a->overload_threshold_ug != b->overload_threshold_ug) ||
        (a->auto_zero_tracking_range_ug != b->auto_zero_tracking_range_ug) ||
        (a->initial_zero_range_permille != b->initial_zero_range_permille) ||
        (a->semi_auto_zero_range_permille != b->semi_auto_zero_range_permille) ||
        (a->compliance_mode != b->compliance_mode) ||
        (a->active_unit != b->active_unit) ||
        (a->enabled_unit_mask != b->enabled_unit_mask) ||
        (a->load_cell.rated_capacity_known != b->load_cell.rated_capacity_known) ||
        (a->load_cell.rated_capacity_ug != b->load_cell.rated_capacity_ug) ||
        (a->load_cell.sensitivity_known != b->load_cell.sensitivity_known) ||
        (a->load_cell.sensitivity_uv_per_v != b->load_cell.sensitivity_uv_per_v) ||
        (a->load_cell.safe_load_known != b->load_cell.safe_load_known) ||
        (a->load_cell.safe_load_permille != b->load_cell.safe_load_permille) ||
        (a->active_profile != b->active_profile)) return false;
    for (i = 0U; i < MASS_UNIT_COUNT; ++i)
        if ((a->unit_display[i].enabled != b->unit_display[i].enabled) ||
            (a->unit_display[i].decimal_places != b->unit_display[i].decimal_places) ||
            (a->unit_display[i].division_digit != b->unit_display[i].division_digit))
            return false;
    for (i = 0U; i < WEIGHING_PROFILE_COUNT; ++i)
        if ((a->profiles[i].sample_rate != b->profiles[i].sample_rate) ||
            (a->profiles[i].gain != b->profiles[i].gain) ||
            (a->profiles[i].filter_mode != b->profiles[i].filter_mode) ||
            (a->profiles[i].filter_strength != b->profiles[i].filter_strength) ||
            (a->profiles[i].stability_window != b->profiles[i].stability_window) ||
            (a->profiles[i].stability_enter_threshold_ug != b->profiles[i].stability_enter_threshold_ug) ||
            (a->profiles[i].stability_exit_threshold_ug != b->profiles[i].stability_exit_threshold_ug) ||
            (a->profiles[i].stability_hold_ms != b->profiles[i].stability_hold_ms))
            return false;
    return true;
}

static bool PersistentCodec_SemanticEqual(const DeviceConfig *a,
    const RuntimeState *ar, const DeviceConfig *b, const RuntimeState *br)
{
    return (a != NULL) && (ar != NULL) && (b != NULL) && (br != NULL) &&
        PersistentCodec_MetrologyEqual(&a->metrology, &b->metrology) &&
        (a->calibration.raw_zero == b->calibration.raw_zero) &&
        (a->calibration.raw_span == b->calibration.raw_span) &&
        (a->calibration.scale_numerator == b->calibration.scale_numerator) &&
        (a->calibration.scale_denominator == b->calibration.scale_denominator) &&
        (a->calibration.calibration_sequence == b->calibration.calibration_sequence) &&
        (a->calibration.calibration_valid == b->calibration.calibration_valid) &&
        (a->calibration.span_mass_ug == b->calibration.span_mass_ug) &&
        (a->stability.window_size == b->stability.window_size) &&
        (a->stability.enter_threshold == b->stability.enter_threshold) &&
        (a->stability.exit_threshold == b->stability.exit_threshold) &&
        (a->stability.stable_hold_ms == b->stability.stable_hold_ms) &&
        (a->communication.baud_rate == b->communication.baud_rate) &&
        (a->communication.parity == b->communication.parity) &&
        (a->communication.stop_bits == b->communication.stop_bits) &&
        (a->communication.modbus_address == b->communication.modbus_address) &&
        (a->communication.protocol_mode == b->communication.protocol_mode) &&
        (a->communication.output_policy == b->communication.output_policy) &&
        (a->communication.output_period_ms == b->communication.output_period_ms) &&
        (a->communication.zero_suppress_range == b->communication.zero_suppress_range) &&
        (a->communication.word_order == b->communication.word_order) &&
        (a->communication.response_delay_ms == b->communication.response_delay_ms) &&
        (a->communication.recommended_poll_interval_ms == b->communication.recommended_poll_interval_ms) &&
        (a->communication.broadcast_write_policy == b->communication.broadcast_write_policy) &&
        (a->communication.pending_apply == b->communication.pending_apply) &&
        (a->bluetooth.uart_baud_rate == b->bluetooth.uart_baud_rate) &&
        (a->bluetooth.protocol_version == b->bluetooth.protocol_version) &&
        (a->bluetooth.w02_configured == b->bluetooth.w02_configured) &&
        (a->alarm.lower_limit_ug == b->alarm.lower_limit_ug) &&
        (a->alarm.upper_limit_ug == b->alarm.upper_limit_ug) &&
        (a->alarm.hysteresis_ug == b->alarm.hysteresis_ug) &&
        (a->alarm.weight_source == b->alarm.weight_source) &&
        (a->alarm.internal_buzzer_enable == b->alarm.internal_buzzer_enable) &&
        (a->alarm.external_buzzer_enable == b->alarm.external_buzzer_enable) &&
        (a->alarm.qualified_beep_enable == b->alarm.qualified_beep_enable) &&
        (a->alarm.limit_function_enable == b->alarm.limit_function_enable) &&
        (a->display.brightness == b->display.brightness) &&
        (a->display.default_weight_view == b->display.default_weight_view) &&
        (a->battery.divider_top_ohm == b->battery.divider_top_ohm) &&
        (a->battery.divider_bottom_ohm == b->battery.divider_bottom_ohm) &&
        (a->battery.calibration_gain_ppm == b->battery.calibration_gain_ppm) &&
        (a->battery.calibration_offset_mv == b->battery.calibration_offset_mv) &&
        (a->battery.low_warning_mv == b->battery.low_warning_mv) &&
        (a->battery.critical_low_mv == b->battery.critical_low_mv) &&
        (a->battery.recovery_mv == b->battery.recovery_mv) &&
        (a->battery.low_voltage_alarm_enable == b->battery.low_voltage_alarm_enable) &&
        (a->system.tare_power_loss_retention == b->system.tare_power_loss_retention) &&
        (a->system.watchdog_enable == b->system.watchdog_enable) &&
        (a->system.startup_auto_zero_enable == b->system.startup_auto_zero_enable) &&
#if (A33_ENABLE_STAGE5PA_PRODUCT != 0U)
        (a->system.requested_r5_mode == b->system.requested_r5_mode) &&
        (a->system.requested_r5_application == b->system.requested_r5_application) &&
        (a->system.requested_checkweigh_mode == b->system.requested_checkweigh_mode) &&
#endif
        (ar->weight_view == br->weight_view) &&
        (ar->current_tare_ug == br->current_tare_ug) &&
        (ar->tare_active == br->tare_active);
}
#endif

bool PersistentCodec_ConfigEqual(const DeviceConfig *left,const RuntimeState *left_runtime,const DeviceConfig *right,const RuntimeState *right_runtime)
{
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    return PersistentCodec_SemanticEqual(left, left_runtime,
                                         right, right_runtime);
#else
    uint8_t a[PERSISTENT_V3_PAYLOAD_SIZE],b[PERSISTENT_V3_PAYLOAD_SIZE];uint16_t al=0U,bl=0U; if(PersistentCodec_EncodeV3(left,left_runtime,a,sizeof(a),&al)!=PERSISTENT_CODEC_OK||PersistentCodec_EncodeV3(right,right_runtime,b,sizeof(b),&bl)!=PERSISTENT_CODEC_OK||al!=bl)return false;return memcmp(a,b,al)==0;
#endif
}
bool PersistentCodec_DeviceConfigEqual(const DeviceConfig *left,const DeviceConfig *right)
{
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    static const RuntimeState empty_runtime = {0};
    return PersistentCodec_ConfigEqual(left, &empty_runtime,
                                       right, &empty_runtime);
#else
    RuntimeState a={0},b={0};
    return PersistentCodec_ConfigEqual(left,&a,right,&b);
#endif
}

PersistentCodecResult PersistentCodec_Decode(uint16_t schema_version,const uint8_t *buffer,uint16_t length,DeviceConfig *config,RuntimeState *runtime)
{(void)schema_version;(void)buffer;(void)length;(void)config;(void)runtime;return PERSISTENT_CODEC_UNSUPPORTED_SCHEMA;}
