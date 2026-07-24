#include "persistent_codec.h"

#include "calibration_model.h"
#include "default_config.h"
#include "metrology_config_validator.h"
#include "persistent_schema.h"
#include "unit_converter.h"

#include <limits.h>
#include <stddef.h>
#include <string.h>

typedef struct
{
    uint8_t *data;
    uint16_t capacity;
    uint16_t position;
    bool failed;
} CodecWriter;

typedef struct
{
    const uint8_t *data;
    uint16_t length;
    uint16_t position;
    bool failed;
} CodecReader;

static void PutU8(CodecWriter *writer, uint8_t value)
{
    if (writer->position >= writer->capacity)
    {
        writer->failed = true;
        return;
    }
    writer->data[writer->position++] = value;
}

static void PutU16(CodecWriter *writer, uint16_t value)
{
    PutU8(writer, (uint8_t)value);
    PutU8(writer, (uint8_t)(value >> 8U));
}

static void PutU32(CodecWriter *writer, uint32_t value)
{
    PutU16(writer, (uint16_t)value);
    PutU16(writer, (uint16_t)(value >> 16U));
}

static void PutU64(CodecWriter *writer, uint64_t value)
{
    PutU32(writer, (uint32_t)value);
    PutU32(writer, (uint32_t)(value >> 32U));
}

static uint8_t GetU8(CodecReader *reader)
{
    if (reader->position >= reader->length)
    {
        reader->failed = true;
        return 0U;
    }
    return reader->data[reader->position++];
}

static uint16_t GetU16(CodecReader *reader)
{
    uint16_t value = GetU8(reader);
    value |= (uint16_t)((uint16_t)GetU8(reader) << 8U);
    return value;
}

static uint32_t GetU32(CodecReader *reader)
{
    uint32_t value = GetU16(reader);
    value |= (uint32_t)GetU16(reader) << 16U;
    return value;
}

static uint64_t GetU64(CodecReader *reader)
{
    uint64_t value = GetU32(reader);
    value |= (uint64_t)GetU32(reader) << 32U;
    return value;
}

static bool GetBool(CodecReader *reader, bool *value)
{
    uint8_t encoded = GetU8(reader);
    if (reader->failed || (encoded > 1U))
    {
        return false;
    }
    *value = encoded != 0U;
    return true;
}

static bool GetReserved(CodecReader *reader, uint8_t count)
{
    uint8_t index;
    for (index = 0U; index < count; ++index)
    {
        if (GetU8(reader) != 0U)
        {
            return false;
        }
    }
    return !reader->failed;
}

bool PersistentCodec_ValidateConfig(const DeviceConfig *config)
{
    int64_t alarm_span;

    if ((config == NULL) ||
        (MetrologyConfig_Validate(&config->metrology, &config->stability) !=
         METROLOGY_CONFIG_OK) ||
        ((uint32_t)config->metrology.sample_mode >= SAMPLE_MODE_COUNT) ||
        ((uint32_t)config->metrology.cs1237_gain >= DEVICE_CS1237_GAIN_COUNT) ||
        ((uint32_t)config->metrology.cs1237_data_rate >=
         DEVICE_CS1237_DATA_RATE_COUNT) ||
        (config->metrology.auto_zero_tracking_range >
         config->metrology.capacity) ||
        (config->calibration.calibration_valid &&
         (CalibrationModel_Validate(&config->calibration) !=
          CALIBRATION_RESULT_OK)) ||
        (config->communication.baud_rate == 0U) ||
        ((uint32_t)config->communication.parity >= COMM_PARITY_COUNT) ||
        ((uint32_t)config->communication.stop_bits >= COMM_STOP_BITS_COUNT) ||
        (config->communication.modbus_address == 0U) ||
        (config->communication.modbus_address > 247U) ||
        ((uint32_t)config->communication.protocol_mode >= PROTOCOL_MODE_COUNT) ||
        ((uint32_t)config->communication.output_policy >= OUTPUT_POLICY_COUNT) ||
        ((uint32_t)config->communication.word_order >= MODBUS_WORD_ORDER_COUNT) ||
        (config->communication.recommended_poll_interval_ms == 0U) ||
        ((config->communication.output_policy == OUTPUT_POLICY_PERIODIC) &&
         (config->communication.output_period_ms == 0U)) ||
        (config->bluetooth.uart_baud_rate == 0U) ||
        (config->bluetooth.protocol_version == 0U) ||
        ((uint32_t)config->alarm.weight_source >= ALARM_WEIGHT_SOURCE_COUNT) ||
        (config->alarm.lower_limit > config->alarm.upper_limit) ||
        (config->alarm.lower_limit_ug > config->alarm.upper_limit_ug) ||
        (config->display.brightness > 7U) ||
        (config->display.default_weight_view >= (uint8_t)WEIGHT_VIEW_COUNT) ||
        (config->battery.divider_top_ohm == 0U) ||
        (config->battery.divider_bottom_ohm == 0U))
    {
        return false;
    }
    alarm_span = (int64_t)config->alarm.upper_limit -
                 (int64_t)config->alarm.lower_limit;
    if (config->alarm.limit_function_enable &&
        ((alarm_span <= 0) ||
         ((uint64_t)config->alarm.hysteresis > (uint64_t)alarm_span) ||
         (config->alarm.hysteresis_ug < 0) ||
         (config->alarm.hysteresis_ug >
          (config->alarm.upper_limit_ug-config->alarm.lower_limit_ug))))
    {
        return false;
    }
    if (config->battery.low_voltage_alarm_enable &&
        ((config->battery.critical_low_mv == 0U) ||
         (config->battery.low_warning_mv <=
          config->battery.critical_low_mv) ||
         (config->battery.recovery_mv < config->battery.low_warning_mv)))
    {
        return false;
    }
    return true;
}

PersistentCodecResult PersistentCodec_EncodeV1(
    const DeviceConfig *config, const RuntimeState *runtime,
    uint8_t *buffer, uint16_t capacity, uint16_t *encoded_length)
{
    CodecWriter w = {buffer, capacity, 0U, false};
    uint8_t index;

    if ((config == NULL) || (runtime == NULL) || (buffer == NULL) ||
        (encoded_length == NULL))
    {
        return PERSISTENT_CODEC_NULL;
    }
    *encoded_length = 0U;
    if (capacity < CONFIG_STORE_V1_PAYLOAD_SIZE)
    {
        return PERSISTENT_CODEC_BUFFER_TOO_SMALL;
    }
    if (!PersistentCodec_ValidateConfig(config) ||
        ((uint32_t)runtime->weight_view >= WEIGHT_VIEW_COUNT))
    {
        return PERSISTENT_CODEC_VALIDATION_FAILED;
    }

#define PUT_BOOL(value) PutU8(&w, (value) ? 1U : 0U)
    PutU32(&w, config->metrology.capacity);
    PutU32(&w, config->metrology.division);
    PutU8(&w, config->metrology.decimal_places);
    PutU8(&w, (uint8_t)config->metrology.unit);
    PutU8(&w, (uint8_t)config->metrology.sample_mode);
    PutU8(&w, (uint8_t)config->metrology.cs1237_gain);
    PutU8(&w, (uint8_t)config->metrology.cs1237_data_rate);
    PutU8(&w, (uint8_t)config->metrology.filter_mode);
    PutU8(&w, config->metrology.filter_strength);
    PutU32(&w, config->metrology.zero_range);
    PutU32(&w, config->metrology.overload_threshold);
    PUT_BOOL(config->metrology.auto_zero_tracking_enable);
    PutU32(&w, config->metrology.auto_zero_tracking_range);

    PutU32(&w, (uint32_t)config->calibration.raw_zero);
    PutU32(&w, (uint32_t)config->calibration.raw_span);
    PutU32(&w, config->calibration.span_weight);
    PutU32(&w, (uint32_t)config->calibration.scale_numerator);
    PutU32(&w, (uint32_t)config->calibration.scale_denominator);
    PutU32(&w, config->calibration.calibration_sequence);
    PUT_BOOL(config->calibration.calibration_valid);

    PutU16(&w, config->stability.window_size);
    PutU32(&w, config->stability.enter_threshold);
    PutU32(&w, config->stability.exit_threshold);
    PutU32(&w, config->stability.stable_hold_ms);

    PutU32(&w, config->communication.baud_rate);
    PutU8(&w, (uint8_t)config->communication.parity);
    PutU8(&w, (uint8_t)config->communication.stop_bits);
    PutU8(&w, config->communication.modbus_address);
    PutU8(&w, (uint8_t)config->communication.protocol_mode);
    PutU8(&w, (uint8_t)config->communication.output_policy);
    PutU32(&w, config->communication.output_period_ms);
    PutU32(&w, config->communication.zero_suppress_range);

    PutU32(&w, config->bluetooth.uart_baud_rate);
    PutU16(&w, config->bluetooth.protocol_version);
    PUT_BOOL(config->bluetooth.w02_configured);
    for (index = 0U; index < 5U; ++index) PutU8(&w, 0U);

    PutU32(&w, (uint32_t)config->alarm.lower_limit);
    PutU32(&w, (uint32_t)config->alarm.upper_limit);
    PutU32(&w, config->alarm.hysteresis);
    PutU8(&w, (uint8_t)config->alarm.weight_source);
    PUT_BOOL(config->alarm.internal_buzzer_enable);
    PUT_BOOL(config->alarm.external_buzzer_enable);
    PUT_BOOL(config->alarm.qualified_beep_enable);
    PUT_BOOL(config->alarm.limit_function_enable);

    PutU8(&w, config->display.brightness);
    PutU8(&w, config->display.default_weight_view);
    for (index = 0U; index < 6U; ++index) PutU8(&w, 0U);

    PutU32(&w, config->battery.divider_top_ohm);
    PutU32(&w, config->battery.divider_bottom_ohm);
    PutU32(&w, (uint32_t)config->battery.calibration_gain_ppm);
    PutU32(&w, (uint32_t)config->battery.calibration_offset_mv);
    PutU32(&w, config->battery.low_warning_mv);
    PutU32(&w, config->battery.critical_low_mv);
    PutU32(&w, config->battery.recovery_mv);
    PUT_BOOL(config->battery.low_voltage_alarm_enable);

    PUT_BOOL(config->system.tare_power_loss_retention);
    PUT_BOOL(config->system.watchdog_enable);
    for (index = 0U; index < 6U; ++index) PutU8(&w, 0U);

    PutU8(&w, (uint8_t)runtime->weight_view);
    PutU32(&w, (uint32_t)runtime->current_tare);
    PUT_BOOL(runtime->tare_active);
#undef PUT_BOOL

    if (w.failed || (w.position != CONFIG_STORE_V1_PAYLOAD_SIZE))
    {
        return PERSISTENT_CODEC_BUFFER_TOO_SMALL;
    }
    *encoded_length = w.position;
    return PERSISTENT_CODEC_OK;
}

PersistentCodecResult PersistentCodec_EncodeV2(
    const DeviceConfig *config, const RuntimeState *runtime,
    uint8_t *buffer, uint16_t capacity, uint16_t *encoded_length)
{
    CodecWriter w;
    DeviceConfig compatibility;
    uint16_t v1_length;
    uint8_t index;
    PersistentCodecResult result;
    if ((config == NULL) || (runtime == NULL) || (buffer == NULL) ||
        (encoded_length == NULL)) return PERSISTENT_CODEC_NULL;
    *encoded_length = 0U;
    if (capacity < PERSISTENT_V2_PAYLOAD_SIZE)
        return PERSISTENT_CODEC_BUFFER_TOO_SMALL;
    compatibility = *config;
    if (compatibility.calibration.calibration_valid &&
        (compatibility.calibration.span_mass_ug > INT32_MAX))
    {
        compatibility.calibration.span_weight = INT32_MAX;
        compatibility.calibration.scale_numerator = INT32_MAX;
    }
    result = PersistentCodec_EncodeV1(&compatibility, runtime, buffer, capacity,
                                      &v1_length);
    if (result != PERSISTENT_CODEC_OK) return result;
    w.data = buffer; w.capacity = capacity; w.position = v1_length; w.failed = false;
#define PUT_BOOL_V2(value) PutU8(&w, (value) ? 1U : 0U)
    PutU64(&w, (uint64_t)config->metrology.capacity_ug);
    PutU64(&w, (uint64_t)config->metrology.verification_interval_e_ug);
    PutU64(&w, (uint64_t)config->metrology.zero_range_ug);
    PutU64(&w, (uint64_t)config->metrology.overload_threshold_ug);
    PutU64(&w, (uint64_t)config->metrology.auto_zero_tracking_range_ug);
    PutU16(&w, config->metrology.initial_zero_range_permille);
    PutU16(&w, config->metrology.semi_auto_zero_range_permille);
    PutU8(&w, (uint8_t)config->metrology.compliance_mode);
    PutU8(&w, (uint8_t)config->metrology.active_unit);
    PutU8(&w, config->metrology.enabled_unit_mask);
    for (index = 0U; index < MASS_UNIT_COUNT; ++index)
    {
        PUT_BOOL_V2(config->metrology.unit_display[index].enabled);
        PutU8(&w, config->metrology.unit_display[index].decimal_places);
        PutU32(&w, config->metrology.unit_display[index].division_digit);
    }
    PUT_BOOL_V2(config->metrology.load_cell.rated_capacity_known);
    PutU64(&w, (uint64_t)config->metrology.load_cell.rated_capacity_ug);
    PUT_BOOL_V2(config->metrology.load_cell.sensitivity_known);
    PutU32(&w, config->metrology.load_cell.sensitivity_uv_per_v);
    PUT_BOOL_V2(config->metrology.load_cell.safe_load_known);
    PutU16(&w, config->metrology.load_cell.safe_load_permille);
    for (index = 0U; index < WEIGHING_PROFILE_COUNT; ++index)
    {
        const WeighingProfileConfig *p = &config->metrology.profiles[index];
        PutU8(&w, (uint8_t)p->sample_rate); PutU8(&w, (uint8_t)p->gain);
        PutU8(&w, (uint8_t)p->filter_mode); PutU8(&w, p->filter_strength);
        PutU8(&w, p->stability_window);
        PutU64(&w, (uint64_t)p->stability_enter_threshold_ug);
        PutU64(&w, (uint64_t)p->stability_exit_threshold_ug);
        PutU32(&w, p->stability_hold_ms);
    }
    PutU8(&w, (uint8_t)config->metrology.active_profile);
    PutU64(&w, (uint64_t)config->calibration.span_mass_ug);
    PutU8(&w, (uint8_t)config->communication.word_order);
    PutU16(&w, config->communication.response_delay_ms);
    PutU16(&w, config->communication.recommended_poll_interval_ms);
    PutU8(&w, config->communication.broadcast_write_policy);
    PUT_BOOL_V2(config->communication.pending_apply);
    PutU64(&w, (uint64_t)config->alarm.lower_limit_ug);
    PutU64(&w, (uint64_t)config->alarm.upper_limit_ug);
    PutU64(&w, (uint64_t)config->alarm.hysteresis_ug);
    PutU64(&w, (uint64_t)runtime->current_tare_ug);
#undef PUT_BOOL_V2
    if (w.failed || (w.position != PERSISTENT_V2_PAYLOAD_SIZE))
        return PERSISTENT_CODEC_BUFFER_TOO_SMALL;
    *encoded_length = w.position;
    return PERSISTENT_CODEC_OK;
}

PersistentCodecResult PersistentCodec_Decode(
    uint16_t schema_version, const uint8_t *buffer, uint16_t length,
    DeviceConfig *config, RuntimeState *runtime)
{
    CodecReader r = {buffer, length, 0U, false};
    uint8_t value;
    bool valid = true;

    if ((buffer == NULL) || (config == NULL) || (runtime == NULL))
        return PERSISTENT_CODEC_NULL;
    if (schema_version == CONFIG_STORE_SCHEMA_V2)
        return PersistentCodec_DecodeV2(buffer, length, config, runtime);
    if (schema_version != CONFIG_STORE_SCHEMA_V1)
        return PERSISTENT_CODEC_UNSUPPORTED_SCHEMA;
    if (length < CONFIG_STORE_V1_PAYLOAD_SIZE)
        return PERSISTENT_CODEC_TRUNCATED;
    if (length != CONFIG_STORE_V1_PAYLOAD_SIZE)
        return PERSISTENT_CODEC_INVALID_VALUE;
    DefaultConfig_Load(config);
    (void)memset(runtime, 0, sizeof(*runtime));

    config->metrology.capacity = GetU32(&r);
    config->metrology.division = GetU32(&r);
    config->metrology.decimal_places = GetU8(&r);
    config->metrology.unit = (WeightUnit)GetU8(&r);
    config->metrology.sample_mode = (SampleMode)GetU8(&r);
    config->metrology.cs1237_gain = (Cs1237Gain)GetU8(&r);
    config->metrology.cs1237_data_rate = (Cs1237DataRate)GetU8(&r);
    config->metrology.filter_mode = (FilterMode)GetU8(&r);
    config->metrology.filter_strength = GetU8(&r);
    config->metrology.zero_range = GetU32(&r);
    config->metrology.overload_threshold = GetU32(&r);
    valid &= GetBool(&r, &config->metrology.auto_zero_tracking_enable);
    config->metrology.auto_zero_tracking_range = GetU32(&r);

    config->calibration.raw_zero = (int32_t)GetU32(&r);
    config->calibration.raw_span = (int32_t)GetU32(&r);
    config->calibration.span_weight = GetU32(&r);
    config->calibration.scale_numerator = (int32_t)GetU32(&r);
    config->calibration.scale_denominator = (int32_t)GetU32(&r);
    config->calibration.calibration_sequence = GetU32(&r);
    valid &= GetBool(&r, &config->calibration.calibration_valid);

    config->stability.window_size = GetU16(&r);
    config->stability.enter_threshold = GetU32(&r);
    config->stability.exit_threshold = GetU32(&r);
    config->stability.stable_hold_ms = GetU32(&r);

    config->communication.baud_rate = GetU32(&r);
    config->communication.parity = (CommunicationParity)GetU8(&r);
    config->communication.stop_bits = (CommunicationStopBits)GetU8(&r);
    config->communication.modbus_address = GetU8(&r);
    config->communication.protocol_mode = (ProtocolMode)GetU8(&r);
    config->communication.output_policy = (OutputPolicy)GetU8(&r);
    config->communication.output_period_ms = GetU32(&r);
    config->communication.zero_suppress_range = GetU32(&r);

    config->bluetooth.uart_baud_rate = GetU32(&r);
    config->bluetooth.protocol_version = GetU16(&r);
    valid &= GetBool(&r, &config->bluetooth.w02_configured);
    valid &= GetReserved(&r, 5U);

    config->alarm.lower_limit = (int32_t)GetU32(&r);
    config->alarm.upper_limit = (int32_t)GetU32(&r);
    config->alarm.hysteresis = GetU32(&r);
    config->alarm.weight_source = (AlarmWeightSource)GetU8(&r);
    valid &= GetBool(&r, &config->alarm.internal_buzzer_enable);
    valid &= GetBool(&r, &config->alarm.external_buzzer_enable);
    valid &= GetBool(&r, &config->alarm.qualified_beep_enable);
    valid &= GetBool(&r, &config->alarm.limit_function_enable);

    config->display.brightness = GetU8(&r);
    config->display.default_weight_view = GetU8(&r);
    valid &= GetReserved(&r, 6U);

    config->battery.divider_top_ohm = GetU32(&r);
    config->battery.divider_bottom_ohm = GetU32(&r);
    config->battery.calibration_gain_ppm = (int32_t)GetU32(&r);
    config->battery.calibration_offset_mv = (int32_t)GetU32(&r);
    config->battery.low_warning_mv = GetU32(&r);
    config->battery.critical_low_mv = GetU32(&r);
    config->battery.recovery_mv = GetU32(&r);
    valid &= GetBool(&r, &config->battery.low_voltage_alarm_enable);

    valid &= GetBool(&r, &config->system.tare_power_loss_retention);
    valid &= GetBool(&r, &config->system.watchdog_enable);
    valid &= GetReserved(&r, 6U);

    value = GetU8(&r);
    runtime->weight_view = (WeightViewMode)value;
    runtime->current_tare = (int32_t)GetU32(&r);
    valid &= GetBool(&r, &runtime->tare_active);

    if (r.failed) return PERSISTENT_CODEC_TRUNCATED;
    if (!valid || (r.position != length)) return PERSISTENT_CODEC_INVALID_VALUE;
    if (!PersistentCodec_ValidateConfig(config) ||
        ((uint32_t)runtime->weight_view >= WEIGHT_VIEW_COUNT))
        return PERSISTENT_CODEC_VALIDATION_FAILED;

    if (!config->system.tare_power_loss_retention || !runtime->tare_active ||
        !config->calibration.calibration_valid || (runtime->current_tare <= 0) ||
        ((uint32_t)runtime->current_tare >
         ((config->metrology.overload_threshold != 0U) ?
          config->metrology.overload_threshold : config->metrology.capacity)))
    {
        runtime->current_tare = 0;
        runtime->tare_active = false;
    }
    /* V1 values are converted to physical mass once, then V2 owns them. */
    if (!UnitConverter_CountToMass(config->metrology.capacity,
            config->metrology.unit, config->metrology.decimal_places,
            &config->metrology.capacity_ug) ||
        !UnitConverter_CountToMass(config->metrology.division,
            config->metrology.unit, config->metrology.decimal_places,
            &config->metrology.verification_interval_e_ug) ||
        !UnitConverter_CountToMass(config->metrology.zero_range,
            config->metrology.unit, config->metrology.decimal_places,
            &config->metrology.zero_range_ug) ||
        !UnitConverter_CountToMass(config->metrology.overload_threshold,
            config->metrology.unit, config->metrology.decimal_places,
            &config->metrology.overload_threshold_ug))
        return PERSISTENT_CODEC_VALIDATION_FAILED;
    config->metrology.active_unit = config->metrology.unit;
    config->metrology.active_profile = WEIGHING_PROFILE_HIGH_PRECISION;
    config->metrology.profiles[0].sample_rate = config->metrology.cs1237_data_rate;
    config->metrology.profiles[0].gain = config->metrology.cs1237_gain;
    config->metrology.profiles[0].filter_mode = config->metrology.filter_mode;
    config->metrology.profiles[0].filter_strength = config->metrology.filter_strength;
    config->metrology.profiles[0].stability_window = (uint8_t)config->stability.window_size;
    (void)UnitConverter_CountToMass(config->stability.enter_threshold,
        config->metrology.unit, config->metrology.decimal_places,
        &config->metrology.profiles[0].stability_enter_threshold_ug);
    (void)UnitConverter_CountToMass(config->stability.exit_threshold,
        config->metrology.unit, config->metrology.decimal_places,
        &config->metrology.profiles[0].stability_exit_threshold_ug);
    config->metrology.profiles[0].stability_hold_ms = config->stability.stable_hold_ms;
    (void)UnitConverter_CountToMass(config->calibration.span_weight,
        config->metrology.unit, config->metrology.decimal_places,
        &config->calibration.span_mass_ug);
    (void)UnitConverter_CountToMass(runtime->current_tare,
        config->metrology.unit, config->metrology.decimal_places,
        &runtime->current_tare_ug);
    runtime->migration_pending_save = true;
    runtime->config_dirty = true;
    return PERSISTENT_CODEC_OK;
}

PersistentCodecResult PersistentCodec_DecodeV2(
    const uint8_t *buffer, uint16_t length,
    DeviceConfig *config, RuntimeState *runtime)
{
    CodecReader r;
    uint8_t index;
    bool valid = true;
    bool encoded_tare_active;
    PersistentCodecResult result;
    if (length != PERSISTENT_V2_PAYLOAD_SIZE)
        return (length < PERSISTENT_V2_PAYLOAD_SIZE) ?
            PERSISTENT_CODEC_TRUNCATED : PERSISTENT_CODEC_INVALID_VALUE;
    encoded_tare_active = buffer[PERSISTENT_V1_PAYLOAD_SIZE - 1U] != 0U;
    result = PersistentCodec_Decode(CONFIG_STORE_SCHEMA_V1, buffer,
        PERSISTENT_V1_PAYLOAD_SIZE, config, runtime);
    if (result != PERSISTENT_CODEC_OK) return result;
    r.data = buffer; r.length = length; r.position = PERSISTENT_V1_PAYLOAD_SIZE;
    r.failed = false;
    config->metrology.capacity_ug = (MassValueUg)GetU64(&r);
    config->metrology.verification_interval_e_ug = (MassValueUg)GetU64(&r);
    config->metrology.zero_range_ug = (MassValueUg)GetU64(&r);
    config->metrology.overload_threshold_ug = (MassValueUg)GetU64(&r);
    config->metrology.auto_zero_tracking_range_ug = (MassValueUg)GetU64(&r);
    config->metrology.initial_zero_range_permille = GetU16(&r);
    config->metrology.semi_auto_zero_range_permille = GetU16(&r);
    config->metrology.compliance_mode = (MetrologyComplianceMode)GetU8(&r);
    config->metrology.active_unit = (MassUnit)GetU8(&r);
    config->metrology.enabled_unit_mask = GetU8(&r);
    for (index = 0U; index < MASS_UNIT_COUNT; ++index)
    {
        valid &= GetBool(&r, &config->metrology.unit_display[index].enabled);
        config->metrology.unit_display[index].decimal_places = GetU8(&r);
        config->metrology.unit_display[index].division_digit =
            (uint8_t)GetU32(&r);
    }
    valid &= GetBool(&r, &config->metrology.load_cell.rated_capacity_known);
    config->metrology.load_cell.rated_capacity_ug = (MassValueUg)GetU64(&r);
    valid &= GetBool(&r, &config->metrology.load_cell.sensitivity_known);
    config->metrology.load_cell.sensitivity_uv_per_v = GetU32(&r);
    valid &= GetBool(&r, &config->metrology.load_cell.safe_load_known);
    config->metrology.load_cell.safe_load_permille = GetU16(&r);
    for (index = 0U; index < WEIGHING_PROFILE_COUNT; ++index)
    {
        WeighingProfileConfig *p = &config->metrology.profiles[index];
        p->sample_rate = (Cs1237DataRate)GetU8(&r); p->gain = (Cs1237Gain)GetU8(&r);
        p->filter_mode = (FilterMode)GetU8(&r); p->filter_strength = GetU8(&r);
        p->stability_window = GetU8(&r);
        p->stability_enter_threshold_ug = (MassValueUg)GetU64(&r);
        p->stability_exit_threshold_ug = (MassValueUg)GetU64(&r);
        p->stability_hold_ms = GetU32(&r);
    }
    config->metrology.active_profile = (WeighingProfileId)GetU8(&r);
    config->calibration.span_mass_ug = (MassValueUg)GetU64(&r);
    config->communication.word_order = (ModbusWordOrder)GetU8(&r);
    config->communication.response_delay_ms = GetU16(&r);
    config->communication.recommended_poll_interval_ms = GetU16(&r);
    config->communication.broadcast_write_policy = GetU8(&r);
    valid &= GetBool(&r, &config->communication.pending_apply);
    config->alarm.lower_limit_ug = (MassValueUg)GetU64(&r);
    config->alarm.upper_limit_ug = (MassValueUg)GetU64(&r);
    config->alarm.hysteresis_ug = (MassValueUg)GetU64(&r);
    runtime->current_tare_ug = (MassValueUg)GetU64(&r);
    if (encoded_tare_active && config->system.tare_power_loss_retention &&
        config->calibration.calibration_valid &&
        (runtime->current_tare_ug > 0) &&
        (runtime->current_tare_ug <= config->metrology.overload_threshold_ug))
        runtime->tare_active = true;
    else
    {
        runtime->current_tare_ug = 0;
        runtime->current_tare = 0;
        runtime->tare_active = false;
    }
    runtime->migration_pending_save = false;
    runtime->config_dirty = false;
    if (r.failed) return PERSISTENT_CODEC_TRUNCATED;
    if (!valid || (r.position != length)) return PERSISTENT_CODEC_INVALID_VALUE;
    return PersistentCodec_ValidateConfig(config) ? PERSISTENT_CODEC_OK :
        PERSISTENT_CODEC_VALIDATION_FAILED;
}

PersistentCodecResult PersistentCodec_MigrateV1ToV2(
    const uint8_t *source, uint16_t source_length,
    DeviceConfig *config, RuntimeState *runtime)
{
    return PersistentCodec_Decode(CONFIG_STORE_SCHEMA_V1, source,
                                  source_length, config, runtime);
}

PersistentCodecResult PersistentCodec_Migrate(
    uint16_t source_schema, const uint8_t *source, uint16_t source_length,
    DeviceConfig *config, RuntimeState *runtime)
{
    if (source_schema != CONFIG_STORE_SCHEMA_V1)
        return PERSISTENT_CODEC_UNSUPPORTED_SCHEMA;
    return PersistentCodec_MigrateV1ToV2(source, source_length,
                                         config, runtime);
}
