#include "ble_command_service.h"
#include "ble_command_protocol.h"
#include "ble_transport.h"
#include "command_service.h"
#include "config_edit.h"
#include "project_config.h"
#include "system_context.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static uint8_t s_rx[1024];
static uint16_t s_rx_length;
static uint16_t s_rx_offset;
static uint8_t s_last_response[BLE_COMMAND_MAX_RESPONSE_FRAME];
static uint16_t s_last_response_length;
static uint32_t s_response_count;
static uint32_t s_execute_count;
static CommandResult s_command_result;
static bool s_priority_accept;
static SystemContext s_context;
static ConfigEditState s_edit_state;
static CalibrationSessionSnapshot s_calibration_snapshot;
static CommandRequest s_last_command_request;

bool BleTransport_Read(uint8_t *data, uint16_t capacity, uint16_t *length)
{
    if ((data == NULL) || (length == NULL) || (capacity == 0U)) return false;
    if (s_rx_offset >= s_rx_length) { *length = 0U; return true; }
    data[0] = s_rx[s_rx_offset++];
    *length = 1U;
    return true;
}

bool BleTransport_WritePriority(const uint8_t *data, uint16_t length)
{
    if (!s_priority_accept) return false;
    (void)memcpy(s_last_response, data, length);
    s_last_response_length = length;
    ++s_response_count;
    return true;
}

bool BleTransport_Write(const uint8_t *data, uint16_t length)
{
    (void)data; (void)length; return false;
}

const SystemContext *SystemContext_Get(void) { return &s_context; }
ConfigEditState ConfigEdit_GetState(void) { return s_edit_state; }

CommandResult CommandService_Execute(const CommandRequest *request,
                                     CommandResponse *response)
{
    ++s_execute_count;
    s_last_command_request = *request;
    (void)memset(response, 0, sizeof(*response));
    response->result = s_command_result;
    if ((request->id == COMMAND_CALIBRATION_BEGIN) &&
        (s_command_result == COMMAND_RESULT_OK))
    {
        s_calibration_snapshot.active = true;
        s_calibration_snapshot.state = CAL_WORKFLOW_WAIT_ZERO_STABLE;
        s_calibration_snapshot.owner = CAL_OWNER_BLE;
        s_calibration_snapshot.session_id = 7U;
        response->value0 = 7;
    }
    if (request->id == COMMAND_REQUEST_CONFIG_SAVE)
        response->result = s_command_result;
    return response->result;
}

bool CommandService_GetCalibrationSnapshot(
    CalibrationSessionSnapshot *snapshot)
{
    if (snapshot == NULL) return false;
    *snapshot = s_calibration_snapshot;
    return true;
}

static void FeedRequest(uint16_t transaction_id, uint8_t operation,
                        const uint8_t *data, uint16_t data_length)
{
    uint8_t frame[BLE_COMMAND_MAX_REQUEST_FRAME];
    uint16_t length = 0U;
    assert(BleCommandProtocol_EncodeRequest(transaction_id, operation, 0U,
        data, data_length, 1U, 10U, frame, sizeof(frame), &length));
    assert((uint32_t)s_rx_length + length <= sizeof(s_rx));
    (void)memcpy(&s_rx[s_rx_length], frame, length);
    s_rx_length = (uint16_t)(s_rx_length + length);
}

static uint8_t ResponseResult(void)
{
    return (s_last_response_length > 15U) ? s_last_response[15] : 0xFFU;
}

static void Reset(void)
{
    (void)memset(s_rx, 0, sizeof(s_rx));
    (void)memset(s_last_response, 0, sizeof(s_last_response));
    (void)memset(&s_context, 0, sizeof(s_context));
    (void)memset(&s_calibration_snapshot, 0,
                 sizeof(s_calibration_snapshot));
    (void)memset(&s_last_command_request, 0, sizeof(s_last_command_request));
    s_calibration_snapshot.state = CAL_WORKFLOW_IDLE;
    s_calibration_snapshot.owner = CAL_OWNER_NONE;
    s_rx_length = 0U;
    s_rx_offset = 0U;
    s_last_response_length = 0U;
    s_response_count = 0U;
    s_execute_count = 0U;
    s_command_result = COMMAND_RESULT_OK;
    s_priority_accept = true;
    s_edit_state = CONFIG_EDIT_IDLE;
    BleCommandService_Init();
}

static void TestDuplicateAndConflict(void)
{
    BleCommandServiceDiagnostics diagnostics;
    uint8_t view = WEIGHT_VIEW_NET;
    Reset();
    FeedRequest(10U, BLE_OPERATION_SET_WEIGHT_VIEW, &view, 1U);
    BleCommandService_Process(10U);
    assert(s_execute_count == 1U && s_response_count == 1U);
    {
        uint8_t cached[BLE_COMMAND_MAX_RESPONSE_FRAME];
        uint16_t cached_length = s_last_response_length;
        (void)memcpy(cached, s_last_response, cached_length);
        FeedRequest(10U, BLE_OPERATION_SET_WEIGHT_VIEW, &view, 1U);
        BleCommandService_Process(11U);
        assert(s_execute_count == 1U && s_response_count == 2U);
        assert(s_last_response_length == cached_length &&
               memcmp(s_last_response, cached, cached_length) == 0);
    }
    view = WEIGHT_VIEW_GROSS;
    FeedRequest(10U, BLE_OPERATION_SET_WEIGHT_VIEW, &view, 1U);
    BleCommandService_Process(12U);
    assert(s_execute_count == 1U && ResponseResult() ==
           BLE_COMMAND_RESULT_TRANSACTION_CONFLICT);
    FeedRequest(10U, BLE_OPERATION_TARE, NULL, 0U);
    BleCommandService_Process(13U);
    assert(s_execute_count == 1U && ResponseResult() ==
           BLE_COMMAND_RESULT_TRANSACTION_CONFLICT);
    view = WEIGHT_VIEW_NET;
    FeedRequest(10U, BLE_OPERATION_SET_WEIGHT_VIEW, &view, 1U);
    BleCommandService_Process(14U);
    assert(s_execute_count == 1U && ResponseResult() == BLE_COMMAND_RESULT_OK);
    BleCommandService_GetDiagnostics(&diagnostics);
    assert(diagnostics.duplicate_requests == 2U &&
           diagnostics.transaction_conflicts == 2U);
}

static void TestDeviceInfoVersion(void)
{
    Reset();
    FeedRequest(11U, BLE_OPERATION_GET_DEVICE_INFO, NULL, 0U);
    BleCommandService_Process(10U);
    assert(s_response_count == 1U);
    assert(s_last_response_length == 34U);
    assert(ResponseResult() == BLE_COMMAND_RESULT_OK);
    assert(s_last_response[20] == BLE_PROTOCOL_VERSION);
    assert(s_last_response[21] == FW_RELEASE_VERSION_MAJOR);
    assert(s_last_response[22] == FW_RELEASE_VERSION_MINOR);
}

static void TestSaveCompletion(void)
{
    AppEvent event = {EVENT_CONFIG_SAVE_COMPLETED, 30U, 0U, 0U, NULL};
    uint32_t before;
    Reset();
    s_command_result = COMMAND_RESULT_ACCEPTED;
    FeedRequest(22U, BLE_OPERATION_SAVE_CONFIG, NULL, 0U);
    BleCommandService_Process(30U);
    assert(s_execute_count == 1U && s_response_count == 0U);
    FeedRequest(22U, BLE_OPERATION_SAVE_CONFIG, NULL, 0U);
    BleCommandService_Process(31U);
    assert(s_execute_count == 1U && s_response_count == 0U);
    BleCommandService_HandleEvent(&event);
    assert(s_response_count == 1U && ResponseResult() == BLE_COMMAND_RESULT_OK);
    before = s_execute_count;
    FeedRequest(22U, BLE_OPERATION_SAVE_CONFIG, NULL, 0U);
    BleCommandService_Process(32U);
    assert(s_execute_count == before && s_response_count == 2U);
}

static void TestQueueRetry(void)
{
    uint8_t view = WEIGHT_VIEW_NET;
    Reset();
    s_priority_accept = false;
    FeedRequest(40U, BLE_OPERATION_SET_WEIGHT_VIEW, &view, 1U);
    BleCommandService_Process(40U);
    assert(s_execute_count == 1U && s_response_count == 0U);
    s_priority_accept = true;
    FeedRequest(40U, BLE_OPERATION_SET_WEIGHT_VIEW, &view, 1U);
    BleCommandService_Process(41U);
    assert(s_execute_count == 1U && s_response_count == 1U);
}

static void TestCalibrationStateAndDispatcher(void)
{
    uint8_t session[2] = {7U, 0U};
    uint8_t mass[10] = {7U, 0U, 0x00U, 0x65U, 0xCDU,
                        0x1DU, 0U, 0U, 0U, 0U};
    uint32_t before;

    Reset();
    FeedRequest(50U, BLE_OPERATION_GET_CALIBRATION_STATE, NULL, 0U);
    BleCommandService_Process(50U);
    assert(s_execute_count == 0U && s_last_response_length == 66U);
    assert(ResponseResult() == BLE_COMMAND_RESULT_OK);
    assert(s_last_response[20] == CAL_WORKFLOW_IDLE);
    assert(s_last_response[21] == CAL_OWNER_NONE);

    FeedRequest(51U, BLE_OPERATION_BEGIN_CALIBRATION, NULL, 0U);
    BleCommandService_Process(51U);
    assert(s_execute_count == 1U);
    assert(s_last_command_request.id == COMMAND_CALIBRATION_BEGIN);
    assert(s_last_response[20] == CAL_WORKFLOW_WAIT_ZERO_STABLE);
    assert(s_last_response[21] == CAL_OWNER_BLE);
    assert(s_last_response[22] == 7U && s_last_response[23] == 0U);

    FeedRequest(52U, BLE_OPERATION_SET_CALIBRATION_MASS, mass,
                (uint16_t)sizeof(mass));
    BleCommandService_Process(52U);
    assert(s_last_command_request.id == COMMAND_CALIBRATION_SET_SPAN_MASS);
    assert(s_last_command_request.flags == 7U);
    assert(s_last_command_request.value64 == INT64_C(500000000));

    FeedRequest(53U, BLE_OPERATION_CAPTURE_CALIBRATION_ZERO, session,
                (uint16_t)sizeof(session));
    BleCommandService_Process(53U);
    assert(s_last_command_request.id == COMMAND_CALIBRATION_CAPTURE_ZERO);
    assert(s_last_command_request.flags == 7U);
    before = s_execute_count;
    FeedRequest(53U, BLE_OPERATION_CAPTURE_CALIBRATION_ZERO, session,
                (uint16_t)sizeof(session));
    BleCommandService_Process(54U);
    assert(s_execute_count == before);
}

int main(void)
{
    TestDeviceInfoVersion();
    TestDuplicateAndConflict();
    TestSaveCompletion();
    TestQueueRetry();
    TestCalibrationStateAndDispatcher();
    puts("BLE command service tests passed");
    return 0;
}
