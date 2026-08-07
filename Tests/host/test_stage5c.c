#include "ble_connection_manager.h"
#include "ble_transport.h"
#include "event_queue.h"
#include "stage5c_ble_diagnostics.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

void Stage5C_FakeReset(void);
void Stage5C_FakeCompleteTx(void);
void Stage5C_FakeRejectTx(bool reject);
uint16_t Stage5C_FakeTxLength(void);
uint8_t Stage5C_FakeTxByte(uint16_t index);
void Stage5C_FakeSetReady(bool ready);
void Stage5C_FakeUartError(void);

#define CHECK(condition) do { if (!(condition)) { \
    printf("FAIL:%d\n", __LINE__); return 1; } } while (0)

static int TestTransport(void)
{
    BleTransportDiagnostics d;
    uint8_t input[3] = {0x00U, 0x7FU, 0xFFU};
    uint8_t output[4] = {0};
    uint16_t length;
    uint8_t tx[10];
    uint16_t index;

    Stage5C_FakeReset();
    BleTransport_Init(0xFFFFFFF0UL);
    CHECK(BleTransport_IsReady());
    BleTransport_GetDiagnostics(&d);
    CHECK(d.rx_bytes == 0U && d.tx_bytes == 0U && d.rx_pending == 0U);

    for (index = 0U; index < sizeof(input); ++index)
        BleTransport_RxPushFromIsr(input[index]);
    BleTransport_Run(5U);
    CHECK(BleTransport_Read(output, 2U, &length) && length == 2U);
    CHECK(output[0] == 0x00U && output[1] == 0x7FU);
    CHECK(BleTransport_Read(output, sizeof(output), &length) && length == 1U);
    CHECK(output[0] == 0xFFU);
    BleTransport_GetDiagnostics(&d);
    CHECK(d.rx_bytes == 3U && d.last_rx_ms == 5U);

    for (index = 0U; index < 600U; ++index)
        BleTransport_RxPushFromIsr((uint8_t)index);
    BleTransport_GetDiagnostics(&d);
    CHECK(d.rx_overflow != 0U && d.rx_pending == (BLE_TRANSPORT_RX_BUFFER_SIZE - 1U));

    (void)memset(tx, 0xA5, sizeof(tx));
    CHECK(BleTransport_Write(tx, sizeof(tx)));
    BleTransport_Run(10U);
    CHECK(Stage5C_FakeTxLength() == sizeof(tx));
    BleTransport_GetDiagnostics(&d);
    CHECK(d.tx_bytes == sizeof(tx) && d.tx_busy && d.tx_pending == 0U);
    Stage5C_FakeCompleteTx();
    BleTransport_Run(11U);
    BleTransport_GetDiagnostics(&d);
    CHECK(d.tx_complete == 1U && !d.tx_busy);

    Stage5C_FakeRejectTx(true);
    CHECK(BleTransport_Write(tx, sizeof(tx)));
    BleTransport_Run(12U);
    BleTransport_GetDiagnostics(&d);
    CHECK(d.tx_pending == sizeof(tx));
    Stage5C_FakeRejectTx(false);
    BleTransport_Run(13U);
    CHECK(Stage5C_FakeTxLength() == sizeof(tx));
    BleTransport_Reset(20U);
    BleTransport_GetDiagnostics(&d);
    CHECK(d.transport_reset_count == 1U && d.rx_pending == 0U && d.tx_pending == 0U);
    return 0;
}

static int TestConnectionManager(void)
{
    BleConnectionDiagnostics d;
    AppEvent event;
    uint8_t byte = 0x42U;

    Stage5C_FakeReset();
    EventQueue_Init();
    BleTransport_Init(100U);
    BleConnectionManager_Init(100U);
    BleConnectionManager_Run(101U);
    BleConnectionManager_GetDiagnostics(&d);
    CHECK(d.module_state == BLE_MODULE_READY);
    CHECK(d.link_state == BLE_LINK_UNKNOWN);
    CHECK(!BleConnectionManager_IsConnected());

    BleTransport_RxPushFromIsr(byte);
    BleConnectionManager_Run(102U);
    BleConnectionManager_GetDiagnostics(&d);
    CHECK(d.module_state == BLE_MODULE_UART_ACTIVE && d.rx_bytes == 1U);
    CHECK(EventQueue_Count() == 2U && EventQueue_Pop(&event));
    CHECK(event.type == EVENT_BLE_STATE_CHANGED);
    CHECK(EventQueue_Pop(&event) && event.type == EVENT_BLE_STATE_CHANGED);
    CHECK(!BleConnectionManager_IsConnected());
    CHECK(d.parse_error == 0U && d.crc_error == 0U);

    BleConnectionManager_Run(1103U);
    BleConnectionManager_GetDiagnostics(&d);
    CHECK(d.module_state == BLE_MODULE_READY);
    CHECK(d.link_state == BLE_LINK_UNKNOWN);
    CHECK(!BleConnectionManager_IsConnected());

    Stage5C_FakeReset();
    EventQueue_Init();
    BleTransport_Init(0xFFFFFFF0UL);
    BleConnectionManager_Init(0xFFFFFFF0UL);
    BleTransport_RxPushFromIsr(byte);
    BleConnectionManager_Run(0xFFFFFFF5UL);
    CHECK(BleConnectionManager_IsReady());
    BleConnectionManager_Run(0x000003DEUL);
    BleConnectionManager_GetDiagnostics(&d);
    CHECK(d.module_state == BLE_MODULE_READY);
    CHECK(d.link_state == BLE_LINK_UNKNOWN);
    return 0;
}

static int TestHardwareDiagnostic(void)
{
    static const uint8_t expected[9] = {
        0x48U, 0x65U, 0x6CU, 0x6CU, 0x6FU, 0x20U, 0x57U, 0x30U, 0x32U
    };
    Stage5CBleTxDiagnosticSnapshot snapshot;
    BleTransportDiagnostics transport;
    uint8_t fill[250];
    uint16_t index;

    Stage5C_FakeReset();
    BleTransport_Init(0U);
    Stage5C_BleDiagnosticsInit();
    CHECK(Stage5C_BleDiagnosticsRequestHello() == STAGE5C_BLE_TX_ACCEPTED);
    snapshot = *Stage5C_BleDiagnosticsGetSnapshot();
    CHECK(snapshot.request_count == 1U && snapshot.accepted_count == 1U);
    CHECK(snapshot.tx_pending_after == 9U && !snapshot.completion_seen);
    CHECK(Stage5C_BleDiagnosticsRequestHello() == STAGE5C_BLE_TX_BUSY);
    BleTransport_Run(1U);
    CHECK(Stage5C_FakeTxLength() == sizeof(expected));
    for (index = 0U; index < sizeof(expected); ++index)
        CHECK(Stage5C_FakeTxByte(index) == expected[index]);
    Stage5C_FakeCompleteTx();
    BleTransport_Run(2U);
    Stage5C_BleDiagnosticsProcess();
    snapshot = *Stage5C_BleDiagnosticsGetSnapshot();
    CHECK(snapshot.completion_seen && snapshot.tx_pending_after == 0U);
    CHECK(snapshot.tx_bytes_after == 9U && snapshot.tx_complete_after == 1U);

    Stage5C_FakeReset();
    BleTransport_Init(10U);
    Stage5C_BleDiagnosticsInit();
    (void)memset(fill, 0x55, sizeof(fill));
    CHECK(BleTransport_Write(fill, sizeof(fill)));
    CHECK(Stage5C_BleDiagnosticsRequestHello() == STAGE5C_BLE_TX_QUEUE_FULL);

    Stage5C_FakeReset();
    Stage5C_FakeSetReady(false);
    BleTransport_Init(20U);
    Stage5C_BleDiagnosticsInit();
    CHECK(Stage5C_BleDiagnosticsRequestHello() == STAGE5C_BLE_TX_NOT_READY);

    Stage5C_FakeReset();
    BleTransport_Init(30U);
    Stage5C_BleDiagnosticsInit();
    CHECK(Stage5C_BleDiagnosticsRequestHello() == STAGE5C_BLE_TX_ACCEPTED);
    BleTransport_Run(31U);
    Stage5C_FakeUartError();
    BleTransport_Run(32U);
    BleTransport_GetDiagnostics(&transport);
    CHECK(transport.uart_error == 1U && !transport.tx_busy);
    CHECK(Stage5C_BleDiagnosticsRequestHello() == STAGE5C_BLE_TX_ACCEPTED);
    return 0;
}

int main(void)
{
    if (TestTransport() != 0) return 1;
    if (TestConnectionManager() != 0) return 1;
    if (TestHardwareDiagnostic() != 0) return 1;
    printf("stage5c host tests passed\n");
    return 0;
}
