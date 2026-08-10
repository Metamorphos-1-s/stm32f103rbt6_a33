#include "ble_checkweigh_codec.h"

#include "ble_frame_codec.h"

#include <stddef.h>

uint8_t BleCheckweighCodec_State(CheckweighState state)
{
    switch (state)
    {
        case CHECKWEIGH_DISABLED: return 0U;
        case CHECKWEIGH_LOW: return 1U;
        case CHECKWEIGH_OK: return 2U;
        case CHECKWEIGH_HIGH: return 3U;
        case CHECKWEIGH_OVERLOAD: return 4U;
        case CHECKWEIGH_FAULT: return 5U;
        default: return 0U;
    }
}

bool BleCheckweighCodec_Encode(const BleCheckweighStatusFields *fields,
    uint8_t *payload, uint16_t capacity, uint16_t *length)
{
    uint8_t flags=0U;
    uint16_t offset=0U;
    if((fields==NULL)||(payload==NULL)||(length==NULL)||
       (capacity<BLE_CHECKWEIGH_PAYLOAD_SIZE)||
       ((uint32_t)fields->weight_source>=ALARM_WEIGHT_SOURCE_COUNT))return false;
    if(fields->limit_enabled)flags|=1U<<0;
    if(fields->stable)flags|=1U<<1;
    if(fields->alarm_active)flags|=1U<<2;
    if(fields->green_active)flags|=1U<<3;
    if(fields->yellow_active)flags|=1U<<4;
    if(fields->red_active)flags|=1U<<5;
    if(fields->internal_buzzer_active)flags|=1U<<6;
    if(fields->external_buzzer_active)flags|=1U<<7;
    BleFrameCodec_PutU8(payload,&offset,BleCheckweighCodec_State(fields->state));
    BleFrameCodec_PutU8(payload,&offset,flags);
    BleFrameCodec_PutU8(payload,&offset,(uint8_t)fields->weight_source);
    BleFrameCodec_PutU8(payload,&offset,0U);
    BleFrameCodec_PutU32(payload,&offset,fields->config_revision);
    *length=offset;
    return true;
}
