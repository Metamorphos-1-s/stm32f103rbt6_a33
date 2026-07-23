#ifndef CS1237_TYPES_H
#define CS1237_TYPES_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    CS1237_RATE_10_HZ = 0,
    CS1237_RATE_40_HZ,
    CS1237_RATE_640_HZ,
    CS1237_RATE_1280_HZ
} CS1237_DataRate;

typedef enum
{
    CS1237_GAIN_1 = 0,
    CS1237_GAIN_2,
    CS1237_GAIN_64,
    CS1237_GAIN_128
} CS1237_Gain;

typedef enum
{
    CS1237_CHANNEL_A = 0,
    CS1237_CHANNEL_TEMPERATURE,
    CS1237_CHANNEL_INTERNAL_SHORT
} CS1237_Channel;

typedef struct
{
    CS1237_DataRate rate;
    CS1237_Gain gain;
    CS1237_Channel channel;
    bool reference_output_enabled;
} CS1237_Config;

typedef struct
{
    int32_t raw;
    uint32_t timestamp_ms;
    uint8_t config_status;
    bool valid;
} CS1237_Sample;

typedef enum
{
    CS1237_STATE_DISABLED = 0,
    CS1237_STATE_POWER_UP_WAIT,
    CS1237_STATE_CONFIGURING,
    CS1237_STATE_SETTLING,
    CS1237_STATE_RUNNING,
    CS1237_STATE_POWER_DOWN,
    CS1237_STATE_ERROR
} CS1237_State;

#endif /* CS1237_TYPES_H */
