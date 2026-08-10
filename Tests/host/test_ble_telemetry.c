#include "ble_frame_codec.h"
#include "ble_checkweigh_codec.h"
#include "ble_telemetry_scheduler.h"
#include "crc16.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct
{
    uint32_t builds;
    uint32_t writes;
    uint32_t bytes;
    bool allow_write;
} TestIo;

static bool Build(uint8_t type, uint16_t sequence, uint32_t timestamp,
                  uint8_t *buffer, uint16_t capacity, uint16_t *length,
                  void *context)
{
    TestIo *io = (TestIo *)context;
    uint8_t payload[BLE_SLOW_PAYLOAD_SIZE];
    uint8_t index;
    (void)sequence;
    (void)timestamp;
    ++io->builds;
    for (index = 0U; index < sizeof(payload); ++index) payload[index] = index;
    return BleFrameCodec_Encode(type, sequence, timestamp,
                                payload,
                                (type==BLE_MESSAGE_FAST_WEIGHT)?BLE_FAST_PAYLOAD_SIZE:
                                ((type==BLE_MESSAGE_SLOW_STATUS)?BLE_SLOW_PAYLOAD_SIZE:
                                 BLE_CHECKWEIGH_PAYLOAD_SIZE), buffer,
                                capacity, length);
}

static bool Write(const uint8_t *data, uint16_t length, void *context)
{
    TestIo *io = (TestIo *)context;
    assert(data != NULL);
    ++io->writes;
    io->bytes += length;
    return io->allow_write;
}

static void TestCodec(void)
{
    uint8_t payload[BLE_FAST_PAYLOAD_SIZE];
    uint8_t frame[BLE_SLOW_FRAME_SIZE];
    uint8_t guard[BLE_SLOW_FRAME_SIZE + 4U];
    uint16_t length = 0U;
    uint16_t offset = 0U;
    uint8_t index;
    for (index = 0U; index < sizeof(payload); ++index) payload[index] = index;
    assert(BleFrameCodec_Encode(BLE_MESSAGE_FAST_WEIGHT, 0x1234U,
        0x78563412U, payload, sizeof(payload), frame, sizeof(frame), &length));
    assert(length == BLE_FAST_FRAME_SIZE);
    assert(frame[0] == 0xA5U && frame[1] == 0x5AU && frame[2] == 1U);
    assert(frame[4] == 42U && frame[5] == 0U && frame[6] == 0x34U &&
           frame[7] == 0x12U && frame[8] == 0x12U && frame[11] == 0x78U);
    assert(frame[length - 2U] ==
           (uint8_t)ProtocolCrc16_Calculate(frame, length - 2U));
    assert(frame[length - 1U] ==
           (uint8_t)(ProtocolCrc16_Calculate(frame, length - 2U) >> 8U));
    assert(ProtocolCrc16_Calculate((const uint8_t *)"123456789", 9U) == 0x4B37U);
    (void)memset(guard, 0xCD, sizeof(guard));
    offset = 0U;
    BleFrameCodec_PutI32(guard, &offset, INT32_MIN);
    BleFrameCodec_PutI64(guard, &offset, INT64_MIN);
    assert(offset == 12U);
    assert(guard[0] == 0x00U && guard[3] == 0x80U && guard[4] == 0x00U &&
           guard[11] == 0x80U && guard[12] == 0xCDU);
    assert(!BleFrameCodec_Encode(BLE_MESSAGE_FAST_WEIGHT, 0U, 0U, payload,
        sizeof(payload), frame, BLE_FAST_FRAME_SIZE - 1U, &length));
    assert(BleFrameCodec_Encode(BLE_MESSAGE_CHECKWEIGH_STATUS,1U,2U,payload,
        BLE_CHECKWEIGH_PAYLOAD_SIZE,frame,sizeof(frame),&length));
    assert(length==BLE_CHECKWEIGH_FRAME_SIZE&&frame[3]==0x03U&&
           frame[4]==BLE_CHECKWEIGH_PAYLOAD_SIZE);
}

static void TestScheduler(void)
{
    BleTelemetryScheduler scheduler;
    BleTelemetryCounters counters;
    TestIo io = {0U, 0U, 0U, true};
    uint32_t now;
    uint8_t index;
    BleTelemetryScheduler_Init(&scheduler, 0xFFFFFF00U);
    for (index = 0U, now = 0xFFFFFF00U; index < 52U; ++index, now += 20U)
        BleTelemetryScheduler_Process(&scheduler, now, true, Build, Write, &io);
    BleTelemetryScheduler_GetCounters(&scheduler, &counters);
    printf("scheduler counts fast=%lu slow=%lu sent=%lu builds=%lu\\n",
           (unsigned long)counters.fast_frames_generated,
           (unsigned long)counters.slow_frames_generated,
           (unsigned long)counters.frames_sent, (unsigned long)io.builds);
    fflush(stdout);
    assert(counters.fast_frames_generated == 5U);
    assert(counters.slow_frames_generated == 1U);
    assert(counters.checkweigh_frames_generated==1U);
    assert(counters.frames_sent == 7U);
    io.allow_write = false;
    BleTelemetryScheduler_Process(&scheduler, 0x000003E8U, true, Build, Write, &io);
    BleTelemetryScheduler_GetCounters(&scheduler, &counters);
    assert(counters.frames_dropped_queue_full == 1U);
    io.allow_write = true;
    BleTelemetryScheduler_Process(&scheduler, 0x000004B0U, false, Build, Write, &io);
    BleTelemetryScheduler_GetCounters(&scheduler, &counters);
    assert(counters.frames_dropped_transport_not_ready >= 1U);
}

static void TestCheckweighPayload(void)
{
    static const CheckweighState states[]={CHECKWEIGH_DISABLED,CHECKWEIGH_LOW,
        CHECKWEIGH_OK,CHECKWEIGH_HIGH,CHECKWEIGH_OVERLOAD,CHECKWEIGH_FAULT};
    BleCheckweighStatusFields fields={0};
    uint8_t payload[BLE_CHECKWEIGH_PAYLOAD_SIZE];
    uint16_t length=0U;
    uint8_t index;
    for(index=0U;index<6U;++index)
    {
        fields.state=states[index];
        assert(BleCheckweighCodec_Encode(&fields,payload,sizeof(payload),&length));
        assert(length==8U&&payload[0]==index);
    }
    fields.state=CHECKWEIGH_HIGH;
    fields.limit_enabled=true;fields.stable=false;fields.alarm_active=true;
    fields.green_active=false;fields.yellow_active=false;fields.red_active=true;
    fields.internal_buzzer_active=false;fields.external_buzzer_active=true;
    fields.weight_source=ALARM_WEIGHT_GROSS;
    fields.config_revision=0x78563412U;
    assert(BleCheckweighCodec_Encode(&fields,payload,sizeof(payload),&length));
    assert(payload[0]==3U&&payload[1]==0xA5U&&payload[2]==1U&&payload[3]==0U);
    assert(payload[4]==0x12U&&payload[5]==0x34U&&payload[6]==0x56U&&
           payload[7]==0x78U);
    fields.internal_buzzer_active=true;fields.external_buzzer_active=false;
    assert(BleCheckweighCodec_Encode(&fields,payload,sizeof(payload),&length));
    assert(payload[1]==0x65U);
    fields.weight_source=ALARM_WEIGHT_SOURCE_COUNT;
    assert(!BleCheckweighCodec_Encode(&fields,payload,sizeof(payload),&length));
}

int main(void)
{
    TestCodec();
    TestScheduler();
    TestCheckweighPayload();
    puts("BLE telemetry codec/scheduler tests passed");
    return 0;
}
