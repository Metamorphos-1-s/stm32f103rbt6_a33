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
    PutU8(&w, (uint8_t)runtime->weight_view); PutU64(&w, (uint64_t)runtime->current_tare_ug); PutU8(&w, runtime->tare_active ? 1U : 0U); PutU8(&w, 0U);
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
    config->alarm.lower_limit_ug=GetI64(&r);config->alarm.upper_limit_ug=GetI64(&r);config->alarm.hysteresis_ug=GetI64(&r);config->alarm.weight_source=(AlarmWeightSource)GetU8(&r);valid&=GetBool(&r,&config->alarm.internal_buzzer_enable);valid&=GetBool(&r,&config->alarm.external_buzzer_enable);valid&=GetBool(&r,&config->alarm.qualified_beep_enable);valid&=GetBool(&r,&config->alarm.limit_function_enable);config->display.brightness=GetU8(&r);config->display.default_weight_view=GetU8(&r);config->battery.divider_top_ohm=GetU32(&r);config->battery.divider_bottom_ohm=GetU32(&r);config->battery.calibration_gain_ppm=GetI32(&r);config->battery.calibration_offset_mv=GetI32(&r);config->battery.low_warning_mv=GetU32(&r);config->battery.critical_low_mv=GetU32(&r);config->battery.recovery_mv=GetU32(&r);valid&=GetBool(&r,&config->battery.low_voltage_alarm_enable);valid&=GetBool(&r,&config->system.tare_power_loss_retention);valid&=GetBool(&r,&config->system.watchdog_enable);valid&=GetBool(&r,&config->system.startup_auto_zero_enable);runtime->weight_view=(WeightViewMode)GetU8(&r);runtime->current_tare_ug=(MassValueUg)GetU64(&r);runtime->current_tare=(runtime->current_tare_ug>INT32_MAX)?INT32_MAX:(int32_t)runtime->current_tare_ug;valid&=GetBool(&r,&runtime->tare_active);valid&=(GetU8(&r)==0U);
    if (r.failed || !valid || (r.position != PERSISTENT_V3_PAYLOAD_SIZE) ||
        !PersistentCodec_ValidateConfig(config) ||
        ((uint32_t)runtime->weight_view >= WEIGHT_VIEW_COUNT))
    {
        return r.failed ? PERSISTENT_CODEC_TRUNCATED :
            PERSISTENT_CODEC_VALIDATION_FAILED;
    }
    return PERSISTENT_CODEC_OK;
}

bool PersistentCodec_ConfigEqual(const DeviceConfig *left,const RuntimeState *left_runtime,const DeviceConfig *right,const RuntimeState *right_runtime)
{ uint8_t a[PERSISTENT_V3_PAYLOAD_SIZE],b[PERSISTENT_V3_PAYLOAD_SIZE];uint16_t al=0U,bl=0U; if(PersistentCodec_EncodeV3(left,left_runtime,a,sizeof(a),&al)!=PERSISTENT_CODEC_OK||PersistentCodec_EncodeV3(right,right_runtime,b,sizeof(b),&bl)!=PERSISTENT_CODEC_OK||al!=bl)return false;return memcmp(a,b,al)==0; }
bool PersistentCodec_DeviceConfigEqual(const DeviceConfig *left,const DeviceConfig *right){RuntimeState a={0},b={0};return PersistentCodec_ConfigEqual(left,&a,right,&b);}

PersistentCodecResult PersistentCodec_Decode(uint16_t schema_version,const uint8_t *buffer,uint16_t length,DeviceConfig *config,RuntimeState *runtime)
{(void)schema_version;(void)buffer;(void)length;(void)config;(void)runtime;return PERSISTENT_CODEC_UNSUPPORTED_SCHEMA;}
