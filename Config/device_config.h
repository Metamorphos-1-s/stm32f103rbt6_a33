#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    WEIGHT_UNIT_KG = 0,
    WEIGHT_UNIT_G,
    WEIGHT_UNIT_LB,
    WEIGHT_UNIT_COUNT
} WeightUnit;

typedef enum
{
    SAMPLE_MODE_NORMAL = 0,
    SAMPLE_MODE_LOW_NOISE,
    SAMPLE_MODE_COUNT
} SampleMode;

typedef enum
{
    CS1237_GAIN_1 = 0,
    CS1237_GAIN_2,
    CS1237_GAIN_64,
    CS1237_GAIN_128,
    CS1237_GAIN_COUNT
} Cs1237Gain;

typedef enum
{
    FILTER_MODE_NONE = 0,
    FILTER_MODE_AVERAGE,
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

typedef struct
{
    uint32_t capacity;
    uint32_t division;
    uint8_t decimal_places;
    WeightUnit unit;
    SampleMode sample_mode;
    Cs1237Gain cs1237_gain;
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
