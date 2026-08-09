#include "uart2_dma_transport.h"

#include <stdint.h>
#include <stdio.h>

void Uart2DmaFakeReset(void);
void Uart2DmaFakeSetPosition(uint16_t position);
void Uart2DmaFakeSetRxCompleteCount(uint32_t count);
void Uart2DmaFakeSetFrameErrorCount(uint32_t count);
void Uart2DmaFakeFill(uint16_t index, uint8_t value);
void Uart2DmaFakePushIdle(uint16_t position, uint32_t timestamp_cycles);
uint32_t Uart2DmaFakeStartRxCount(void);

#define CHECK(condition) do { if (!(condition)) { \
    printf("FAIL:%d\n", __LINE__); return 1; } } while (0)

int main(void)
{
    BspUart2Config config = {115200U, COMM_PARITY_NONE, COMM_STOP_BITS_1};
    const Uart2DmaTransportStatistics *statistics;
    uint8_t byte;
    uint16_t index;

    Uart2DmaFakeReset();
    Uart2DmaFakeSetRxCompleteCount(13U);
    CHECK(Uart2DmaTransport_Init(&config));
    CHECK(Uart2DmaFakeStartRxCount() == 1U);

    Uart2DmaFakeSetPosition(12U);
    Uart2DmaFakeSetFrameErrorCount(1U);
    Uart2DmaTransport_Process();
    CHECK(Uart2DmaTransport_TakeReceiveError());
    CHECK(Uart2DmaFakeStartRxCount() == 2U);
    statistics = Uart2DmaTransport_GetStatistics();
    CHECK(statistics->uart_frame_error_count == 1U);
    CHECK(statistics->rx_overrun_count == 0U);

    Uart2DmaFakeReset();
    CHECK(Uart2DmaTransport_Init(&config));
    for (index = 0U; index < 16U; ++index)
        Uart2DmaFakeFill(index, (uint8_t)(0xB0U + index));
    Uart2DmaFakePushIdle(8U, 100U);
    Uart2DmaFakePushIdle(16U, 200U);
    Uart2DmaFakeSetPosition(16U);
    Uart2DmaTransport_Process();
    for (index = 0U; index < 8U; ++index)
    {
        CHECK(Uart2DmaTransport_TryReadByte(&byte));
        CHECK(byte == (uint8_t)(0xB0U + index));
    }
    CHECK(!Uart2DmaTransport_TryReadByte(&byte));
    {
        uint16_t idle_position;
        uint32_t idle_timestamp;
        CHECK(Uart2DmaTransport_TakeIdleEvent(
            &idle_position, &idle_timestamp));
        CHECK(idle_position == 8U && idle_timestamp == 100U);
    }
    for (index = 8U; index < 16U; ++index)
    {
        CHECK(Uart2DmaTransport_TryReadByte(&byte));
        CHECK(byte == (uint8_t)(0xB0U + index));
    }
    CHECK(!Uart2DmaTransport_TryReadByte(&byte));
    {
        uint16_t idle_position;
        uint32_t idle_timestamp;
        CHECK(Uart2DmaTransport_TakeIdleEvent(
            &idle_position, &idle_timestamp));
        CHECK(idle_position == 16U && idle_timestamp == 200U);
    }

    Uart2DmaTransport_Process();
    CHECK(!Uart2DmaTransport_TakeReceiveError());
    CHECK(Uart2DmaFakeStartRxCount() == 2U);

    for (index = 0U; index < 8U; ++index)
        Uart2DmaFakeFill(index, (uint8_t)(0xA0U + index));
    Uart2DmaFakeSetPosition(8U);
    Uart2DmaTransport_Process();
    CHECK(!Uart2DmaTransport_TakeReceiveError());
    for (index = 0U; index < 8U; ++index)
    {
        CHECK(Uart2DmaTransport_TryReadByte(&byte));
        CHECK(byte == (uint8_t)(0xA0U + index));
    }
    CHECK(!Uart2DmaTransport_TryReadByte(&byte));
    statistics = Uart2DmaTransport_GetStatistics();
    CHECK(statistics->rx_byte_count == 8U);
    CHECK(statistics->rx_overrun_count == 0U);
    printf("uart2 dma recovery host tests passed\n");
    return 0;
}
