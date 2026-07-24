#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include "mass_types.h"
#include "unit_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef MassUnit WeightUnit;
#define WEIGHT_UNIT_KG MASS_UNIT_KG
#define WEIGHT_UNIT_G MASS_UNIT_G
#define WEIGHT_UNIT_LB MASS_UNIT_LB
#define WEIGHT_UNIT_COUNT MASS_UNIT_COUNT

typedef enum
{
    SAMPLE_MODE_NORMAL = 0,
    SAMPLE_MODE_LOW_NOISE,
    SAMPLE_MODE_COUNT
} SampleMode;

typedef enum
{
    DEVICE_CS1237_GAIN_1 = 0,
    DEVICE_CS1237_GAIN_2,
    DEVICE_CS1237_GAIN_64,
    DEVICE_CS1237_GAIN_128,
    DEVICE_CS1237_GAIN_COUNT
} Cs1237Gain;

typedef enum
{
    DEVICE_CS1237_DATA_RATE_10_HZ = 0,
    DEVICE_CS1237_DATA_RATE_40_HZ,
    DEVICE_CS1237_DATA_RATE_640_HZ,
    DEVICE_CS1237_DATA_RATE_1280_HZ,
    DEVICE_CS1237_DATA_RATE_COUNT
} Cs1237DataRate;

typedef enum
{
    FILTER_MODE_NONE = 0,
    FILTER_MODE_AVERAGE,
    FILTER_MODE_IIR,
    FILTER_MODE_MEDIAN3_IIR,
    FILTER_MODE_COUNT
} FilterMode;

typedef enum
{
    COMM_PARITY_NONE = 0,
    COMM_PARITY_EVEN,
    COMM_PARITY_ODD,
    COMM_PARITY_COUNT
} CommunicationParity;

typedef enum
{
    COMM_STOP_BITS_1 = 0,
    COMM_STOP_BITS_2,
    COMM_STOP_BITS_COUNT
} CommunicationStopBits;

typedef enum
{
    PROTOCOL_MODE_NONE = 0,
    PROTOCOL_MODE_MODBUS_RTU,
    PROTOCOL_MODE_CUSTOM,
    PROTOCOL_MODE_COUNT
} ProtocolMode;

typedef enum
{
    OUTPUT_POLICY_ON_REQUEST = 0,
    OUTPUT_POLICY_PERIODIC,
    OUTPUT_POLICY_COUNT
} OutputPolicy;

typedef enum
{
    ALARM_WEIGHT_NET = 0,
    ALARM_WEIGHT_GROSS,
    ALARM_WEIGHT_SOURCE_COUNT
} AlarmWeightSource;

typedef enum
{
    METROLOGY_COMPLIANCE_GENERAL = 0,
    METROLOGY_COMPLIANCE_CLASS_III_REFERENCE,
    METROLOGY_COMPLIANCE_COUNT
} MetrologyComplianceMode;

typedef enum
{
    WEIGHING_PROFILE_HIGH_PRECISION = 0,
    WEIGHING_PROFILE_HIGH_SPEED,
    WEIGHING_PROFILE_COUNT
} WeighingProfileId;

typedef enum
{
    MODBUS_WORD_ORDER_HIGH_WORD_FIRST = 0,
    MODBUS_WORD_ORDER_LOW_WORD_FIRST,
    MODBUS_WORD_ORDER_COUNT
} ModbusWordOrder;

typedef struct
{
    bool rated_capacity_known;
    MassValueUg rated_capacity_ug;
    bool sensitivity_known;
    uint32_t sensitivity_uv_per_v;
    bool safe_load_known;
    uint16_t safe_load_permille;
} LoadCellMetadata;

typedef struct
{
    Cs1237DataRate sample_rate;
    Cs1237Gain gain;
    FilterMode filter_mode;
    uint8_t filter_strength;
    uint8_t stability_window;
    MassValueUg stability_enter_threshold_ug;
    MassValueUg stability_exit_threshold_ug;
    uint32_t stability_hold_ms;
} WeighingProfileConfig;

typedef struct
{
    MassValueUg capacity_ug;
    MassValueUg verification_interval_e_ug;
    MassValueUg zero_range_ug;
    MassValueUg overload_threshold_ug;
    MassValueUg auto_zero_tracking_range_ug;
    uint16_t initial_zero_range_permille;
    uint16_t semi_auto_zero_range_permille;
    MetrologyComplianceMode compliance_mode;
    MassUnit active_unit;
    uint8_t enabled_unit_mask;
    UnitDisplayConfig unit_display[MASS_UNIT_COUNT];
    LoadCellMetadata load_cell;
    WeighingProfileConfig profiles[WEIGHING_PROFILE_COUNT];
    WeighingProfileId active_profile;
    /* Legacy V1 fields retained only for migration compatibility. */
    uint32_t capacity;
    uint32_t division;
    uint8_t decimal_places;
    WeightUnit unit;
    SampleMode sample_mode;
    Cs1237Gain cs1237_gain;
    Cs1237DataRate cs1237_data_rate;
    FilterMode filter_mode;
    uint8_t filter_strength;
    uint32_t zero_range;
    uint32_t overload_threshold;
    bool auto_zero_tracking_enable;
    uint32_t auto_zero_tracking_range;
} MetrologyConfig;

typedef struct
{
    int32_t raw_zero;
    int32_t raw_span;
    uint32_t span_weight;
    int32_t scale_numerator;
    int32_t scale_denominator;
    uint32_t calibration_sequence;
    bool calibration_valid;
    MassValueUg span_mass_ug;
} CalibrationConfig;

typedef struct
{
    uint16_t window_size;
    uint32_t enter_threshold;
    uint32_t exit_threshold;
    uint32_t stable_hold_ms;
} StabilityConfig;

typedef struct
{
    uint32_t baud_rate;
    CommunicationParity parity;
    CommunicationStopBits stop_bits;
    uint8_t modbus_address;
    ProtocolMode protocol_mode;
    OutputPolicy output_policy;
    uint32_t output_period_ms;
    uint32_t zero_suppress_range;
    ModbusWordOrder word_order;
    uint16_t response_delay_ms;
    uint16_t recommended_poll_interval_ms;
    uint8_t broadcast_write_policy;
    bool pending_apply;
} CommunicationConfig;

typedef struct
{
    uint32_t uart_baud_rate;
    uint16_t protocol_version;
    bool w02_configured;
    uint8_t reserved[5];
} BluetoothConfig;

typedef struct
{
    int32_t lower_limit;
    int32_t upper_limit;
    uint32_t hysteresis;
    AlarmWeightSource weight_source;
    bool internal_buzzer_enable;
    bool external_buzzer_enable;
    bool qualified_beep_enable;
    bool limit_function_enable;
    MassValueUg lower_limit_ug;
    MassValueUg upper_limit_ug;
    MassValueUg hysteresis_ug;
} AlarmConfig;

typedef struct
{
    uint8_t brightness;
    uint8_t default_weight_view;
    uint8_t reserved[6];
} DisplayConfig;

typedef struct
{
    uint32_t divider_top_ohm;
    uint32_t divider_bottom_ohm;
    int32_t calibration_gain_ppm;
    int32_t calibration_offset_mv;
    uint32_t low_warning_mv;
    uint32_t critical_low_mv;
    uint32_t recovery_mv;
    bool low_voltage_alarm_enable;
} BatteryConfig;

typedef struct
{
    bool tare_power_loss_retention;
    bool watchdog_enable;
    uint8_t reserved[6];
} SystemConfig;

typedef struct
{
    MetrologyConfig metrology;
    CalibrationConfig calibration;
    StabilityConfig stability;
    CommunicationConfig communication;
    BluetoothConfig bluetooth;
    AlarmConfig alarm;
    DisplayConfig display;
    BatteryConfig battery;
    SystemConfig system;
} DeviceConfig;

#endif
