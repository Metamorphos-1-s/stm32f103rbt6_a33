#ifndef BLE_CHECKWEIGH_CODEC_H
#define BLE_CHECKWEIGH_CODEC_H

#include "limit_checker.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    CheckweighState state;
    bool limit_enabled;
    bool stable;
    bool alarm_active;
    bool green_active;
    bool yellow_active;
    bool red_active;
    bool internal_buzzer_active;
    bool external_buzzer_active;
    AlarmWeightSource weight_source;
    uint32_t config_revision;
} BleCheckweighStatusFields;

uint8_t BleCheckweighCodec_State(CheckweighState state);
bool BleCheckweighCodec_Encode(const BleCheckweighStatusFields *fields,
    uint8_t *payload, uint16_t capacity, uint16_t *length);

#endif /* BLE_CHECKWEIGH_CODEC_H */
