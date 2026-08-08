#include "ble_command_service.h"

#include "ble_command_protocol.h"
#include "ble_frame_codec.h"
#include "ble_transport.h"
#include "command_service.h"
#include "config_edit.h"
#include "modbus_register_map.h"
#include "project_config.h"
#include "system_context.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define BLE_COMMAND_RX_BYTES_PER_RUN 64U
#define BLE_COMMAND_FRAMES_PER_RUN 2U
#define BLE_COMMAND_CACHE_DEPTH 4U

typedef struct
{
    bool valid;
    bool pending;
    uint16_t transaction_id;
    uint8_t operation;
    uint8_t request_flags;
    uint16_t request_length;
    uint8_t request_data[BLE_COMMAND_MAX_REQUEST_DATA];
    uint16_t response_length;
    uint8_t response[BLE_COMMAND_MAX_RESPONSE_FRAME];
} CachedResponse;

static BleCommandParser s_parser;
static CachedResponse s_cache[BLE_COMMAND_CACHE_DEPTH];
static uint8_t s_cache_next;
static int8_t s_pending_save_index;
static uint16_t s_response_sequence;
static BleCommandServiceDiagnostics s_diagnostics;
static uint32_t s_now_ms;
static bool s_initialized;

static CachedResponse *FindCache(const BleCommandRequest *request)
{
    uint8_t index;
    for (index = 0U; index < BLE_COMMAND_CACHE_DEPTH; ++index)
    {
        CachedResponse *entry = &s_cache[index];
        if (entry->valid && (entry->transaction_id == request->transaction_id))
        {
            if ((entry->operation == request->operation) &&
                (entry->request_flags == request->flags) &&
                (entry->request_length == request->data_length) &&
                ((request->data_length == 0U) ||
                 (memcmp(entry->request_data, request->data,
                         request->data_length) == 0)))
                return entry;
            return (CachedResponse *)(uintptr_t)1U;
        }
    }
    return NULL;
}

static CachedResponse *AllocateCache(void)
{
    uint8_t index;
    for (index = 0U; index < BLE_COMMAND_CACHE_DEPTH; ++index)
    {
        uint8_t candidate = (uint8_t)((s_cache_next + index) %
            BLE_COMMAND_CACHE_DEPTH);
        if (!s_cache[candidate].valid || !s_cache[candidate].pending)
        {
            s_cache_next = (uint8_t)((candidate + 1U) %
                BLE_COMMAND_CACHE_DEPTH);
            return &s_cache[candidate];
        }
    }
    return NULL;
}

static BleCommandResult MapResult(CommandResult result)
{
    switch (result)
    {
        case COMMAND_RESULT_OK: return BLE_COMMAND_RESULT_OK;
        case COMMAND_RESULT_NOT_IMPLEMENTED: return BLE_COMMAND_RESULT_UNSUPPORTED;
        case COMMAND_RESULT_INVALID_ARGUMENT: return BLE_COMMAND_RESULT_INVALID_ARGUMENT;
        case COMMAND_RESULT_INVALID_STATE: return BLE_COMMAND_RESULT_INVALID_STATE;
        case COMMAND_RESULT_NOT_STABLE: return BLE_COMMAND_RESULT_NOT_STABLE;
        case COMMAND_RESULT_NOT_CALIBRATED: return BLE_COMMAND_RESULT_CALIBRATION_INVALID;
        case COMMAND_RESULT_OUT_OF_ZERO_RANGE: return BLE_COMMAND_RESULT_OUT_OF_RANGE;
        case COMMAND_RESULT_OVERLOAD: return BLE_COMMAND_RESULT_OVERLOAD;
        case COMMAND_RESULT_BUSY: return BLE_COMMAND_RESULT_BUSY;
        case COMMAND_RESULT_POWER_UNSAFE: return BLE_COMMAND_RESULT_POWER_UNSAFE;
        case COMMAND_RESULT_STORAGE_UNAVAILABLE: return BLE_COMMAND_RESULT_PERSISTENCE_FAILED;
        case COMMAND_RESULT_TARE_ACTIVE: return BLE_COMMAND_RESULT_TARE_ACTIVE;
        case COMMAND_RESULT_ZERO_DISABLED: return BLE_COMMAND_RESULT_ZERO_DISABLED;
        case COMMAND_RESULT_CALIBRATION_INVALID:
            return BLE_COMMAND_RESULT_CALIBRATION_INVALID;
        case COMMAND_RESULT_ACCEPTED: return BLE_COMMAND_RESULT_OK;
        case COMMAND_RESULT_INTERNAL_ERROR:
        default: return BLE_COMMAND_RESULT_INTERNAL_ERROR;
    }
}

static uint32_t Capabilities(void)
{
    return (1UL << 0) | (1UL << 1) | (1UL << 2) | (1UL << 3) |
        (1UL << 4) | (1UL << 5) | (1UL << 6) | (1UL << 7);
}

static uint16_t BuildDeviceInfo(uint8_t *data)
{
    uint16_t offset = 0U;
    BleFrameCodec_PutU8(data, &offset, BLE_PROTOCOL_VERSION);
    BleFrameCodec_PutU8(data, &offset, FW_RELEASE_VERSION_MAJOR);
    BleFrameCodec_PutU8(data, &offset, FW_RELEASE_VERSION_MINOR);
    BleFrameCodec_PutU8(data, &offset, 0U);
    BleFrameCodec_PutU16(data, &offset, DEVICE_CONFIG_SCHEMA_VERSION);
    BleFrameCodec_PutU16(data, &offset, MODBUS_REGISTER_MAP_VERSION);
    BleFrameCodec_PutU32(data, &offset, Capabilities());
    return offset;
}

static uint16_t BuildActiveConfig(uint8_t *data)
{
    const SystemContext *context = SystemContext_Get();
    const MetrologyConfig *metrology;
    const UnitDisplayConfig *display;
    const WeighingProfileConfig *profile;
    uint16_t offset = 0U;
    if (context == NULL) return 0U;
    metrology = &context->config.metrology;
    display = &metrology->unit_display[metrology->active_unit];
    profile = &metrology->profiles[metrology->active_profile];
    BleFrameCodec_PutU8(data, &offset, (uint8_t)metrology->active_unit);
    BleFrameCodec_PutU8(data, &offset, display->decimal_places);
    BleFrameCodec_PutU8(data, &offset, display->division_digit);
    BleFrameCodec_PutU8(data, &offset, (uint8_t)metrology->active_profile);
    BleFrameCodec_PutI64(data, &offset, metrology->capacity_ug);
    BleFrameCodec_PutI64(data, &offset, metrology->overload_threshold_ug);
    BleFrameCodec_PutI64(data, &offset, metrology->zero_range_ug);
    BleFrameCodec_PutU8(data, &offset, (uint8_t)profile->filter_mode);
    BleFrameCodec_PutU8(data, &offset, profile->filter_strength);
    BleFrameCodec_PutU8(data, &offset, profile->stability_window);
    BleFrameCodec_PutU8(data, &offset, (uint8_t)profile->sample_rate);
    BleFrameCodec_PutU8(data, &offset, (uint8_t)profile->gain);
    BleFrameCodec_PutI64(data, &offset, profile->stability_enter_threshold_ug);
    BleFrameCodec_PutI64(data, &offset, profile->stability_exit_threshold_ug);
    BleFrameCodec_PutU32(data, &offset, profile->stability_hold_ms);
    BleFrameCodec_PutU8(data, &offset, context->runtime.config_dirty ? 1U : 0U);
    BleFrameCodec_PutU8(data, &offset,
        (uint8_t)ConfigEdit_GetState());
    return offset;
}

static uint16_t BuildCalibrationState(uint8_t *data)
{
    CalibrationSessionSnapshot snapshot;
    uint16_t offset = 0U;
    uint8_t flags = 0U;

    if ((data == NULL) ||
        !CommandService_GetCalibrationSnapshot(&snapshot))
        return 0U;
    if (snapshot.active) flags |= 1U << 0;
    if (snapshot.stable) flags |= 1U << 1;
    if (snapshot.zero_captured) flags |= 1U << 2;
    if (snapshot.load_captured) flags |= 1U << 3;
    if (snapshot.candidate_valid) flags |= 1U << 4;
    if (snapshot.result_valid) flags |= 1U << 5;
    if (snapshot.persistent_dirty) flags |= 1U << 6;
    BleFrameCodec_PutU8(data, &offset, (uint8_t)snapshot.state);
    BleFrameCodec_PutU8(data, &offset, (uint8_t)snapshot.owner);
    BleFrameCodec_PutU16(data, &offset, snapshot.session_id);
    BleFrameCodec_PutU8(data, &offset, (uint8_t)snapshot.locked_unit);
    BleFrameCodec_PutU8(data, &offset, snapshot.locked_decimal_places);
    BleFrameCodec_PutU8(data, &offset, snapshot.locked_division_digit);
    BleFrameCodec_PutU8(data, &offset, flags);
    BleFrameCodec_PutI64(data, &offset, snapshot.calibration_mass_ug);
    BleFrameCodec_PutI64(data, &offset, snapshot.locked_capacity_ug);
    BleFrameCodec_PutI32(data, &offset, snapshot.zero_raw);
    BleFrameCodec_PutI32(data, &offset, snapshot.load_raw);
    BleFrameCodec_PutI32(data, &offset, snapshot.span_raw);
    BleFrameCodec_PutU32(data, &offset, snapshot.sample_sequence);
    BleFrameCodec_PutU8(data, &offset, snapshot.sample_count);
    BleFrameCodec_PutU8(data, &offset, (uint8_t)snapshot.last_result);
    BleFrameCodec_PutU16(data, &offset, 0U);
    return offset;
}

static bool IsCalibrationOperation(uint8_t operation)
{
    return (operation >= BLE_OPERATION_GET_CALIBRATION_STATE) &&
           (operation <= BLE_OPERATION_CANCEL_CALIBRATION);
}

static bool IsKnownOperation(uint8_t operation)
{
    switch (operation)
    {
        case BLE_OPERATION_GET_DEVICE_INFO:
        case BLE_OPERATION_GET_ACTIVE_CONFIG:
        case BLE_OPERATION_TARE:
        case BLE_OPERATION_CLEAR_TARE:
        case BLE_OPERATION_ZERO:
        case BLE_OPERATION_RESET_ZERO:
        case BLE_OPERATION_SET_WEIGHT_VIEW:
        case BLE_OPERATION_SET_DISPLAY_UNIT:
        case BLE_OPERATION_BEGIN_CONFIG_EDIT:
        case BLE_OPERATION_SET_CONFIG_MASS:
        case BLE_OPERATION_SET_UNIT_DISPLAY:
        case BLE_OPERATION_SET_PROFILE_FIELD:
        case BLE_OPERATION_VALIDATE_CONFIG:
        case BLE_OPERATION_APPLY_CONFIG:
        case BLE_OPERATION_DISCARD_CONFIG:
        case BLE_OPERATION_SAVE_CONFIG:
        case BLE_OPERATION_GET_CALIBRATION_STATE:
        case BLE_OPERATION_BEGIN_CALIBRATION:
        case BLE_OPERATION_SET_CALIBRATION_MASS:
        case BLE_OPERATION_CAPTURE_CALIBRATION_ZERO:
        case BLE_OPERATION_CAPTURE_CALIBRATION_LOAD:
        case BLE_OPERATION_APPLY_CALIBRATION:
        case BLE_OPERATION_CANCEL_CALIBRATION:
            return true;
        default:
            return false;
    }
}

static bool ExecuteCommand(const BleCommandRequest *request,
                           CommandResponse *response,
                           BleCommandResult *result)
{
    CommandRequest command = {0};
    uint8_t field;
    if ((request == NULL) || (response == NULL) || (result == NULL)) return false;
    command.source = COMMAND_SOURCE_BLE;
    switch (request->operation)
    {
        case BLE_OPERATION_TARE:
            if (request->data_length != 0U) return false;
            command.id = COMMAND_TARE; break;
        case BLE_OPERATION_CLEAR_TARE:
            if (request->data_length != 0U) return false;
            command.id = COMMAND_CLEAR_TARE; break;
        case BLE_OPERATION_ZERO:
            if (request->data_length != 0U) return false;
            command.id = COMMAND_ZERO; break;
        case BLE_OPERATION_RESET_ZERO:
            if (request->data_length != 0U) return false;
            command.id = COMMAND_RESET_ZERO; break;
        case BLE_OPERATION_SET_WEIGHT_VIEW:
            if (request->data_length != 1U) return false;
            command.id = COMMAND_SET_WEIGHT_VIEW;
            command.value0 = request->data[0]; break;
        case BLE_OPERATION_SET_DISPLAY_UNIT:
            if (request->data_length != 1U) return false;
            command.id = COMMAND_SET_DISPLAY_UNIT;
            command.value0 = request->data[0]; break;
        case BLE_OPERATION_BEGIN_CONFIG_EDIT:
            if (request->data_length != 0U) return false;
            command.id = COMMAND_BEGIN_CONFIG_EDIT; break;
        case BLE_OPERATION_SET_CONFIG_MASS:
            if (request->data_length != 9U) return false;
            field = request->data[0];
            command.id = COMMAND_SET_CONFIG_MASS_FIELD;
            command.value0 = field;
            command.value64 = BleCommandProtocol_GetI64(&request->data[1]);
            break;
        case BLE_OPERATION_SET_UNIT_DISPLAY:
            if (request->data_length != 3U) return false;
            command.id = COMMAND_SET_UNIT_DISPLAY_CONFIG;
            command.value0 = request->data[0];
            command.value1 = request->data[1];
            command.flags = request->data[2];
            break;
        case BLE_OPERATION_SET_PROFILE_FIELD:
            if (request->data_length != 10U) return false;
            field = request->data[1];
            if ((field == CONFIG_PROFILE_FIELD_SAMPLE_RATE) ||
                (field == CONFIG_PROFILE_FIELD_GAIN)) return false;
            command.id = COMMAND_SET_PROFILE_FIELD;
            command.value0 = request->data[0];
            command.value1 = field;
            command.value64 = BleCommandProtocol_GetI64(&request->data[2]);
            break;
        case BLE_OPERATION_VALIDATE_CONFIG:
            if (request->data_length != 0U) return false;
            command.id = COMMAND_CONFIG_VALIDATE; break;
        case BLE_OPERATION_APPLY_CONFIG:
            if (request->data_length != 0U) return false;
            command.id = COMMAND_COMMIT_CONFIG_EDIT; break;
        case BLE_OPERATION_DISCARD_CONFIG:
            if (request->data_length != 0U) return false;
            command.id = COMMAND_CANCEL_CONFIG_EDIT; break;
        case BLE_OPERATION_SAVE_CONFIG:
            if (request->data_length != 0U) return false;
            command.id = COMMAND_REQUEST_CONFIG_SAVE; break;
        case BLE_OPERATION_BEGIN_CALIBRATION:
            if (request->data_length != 0U) return false;
            command.id = COMMAND_CALIBRATION_BEGIN; break;
        case BLE_OPERATION_SET_CALIBRATION_MASS:
            if (request->data_length != 10U) return false;
            command.id = COMMAND_CALIBRATION_SET_SPAN_MASS;
            command.flags = BleCommandProtocol_GetU16(request->data);
            command.value64 = BleCommandProtocol_GetI64(&request->data[2]);
            break;
        case BLE_OPERATION_CAPTURE_CALIBRATION_ZERO:
            if (request->data_length != 2U) return false;
            command.id = COMMAND_CALIBRATION_CAPTURE_ZERO;
            command.flags = BleCommandProtocol_GetU16(request->data);
            break;
        case BLE_OPERATION_CAPTURE_CALIBRATION_LOAD:
            if (request->data_length != 2U) return false;
            command.id = COMMAND_CALIBRATION_CAPTURE_SPAN;
            command.flags = BleCommandProtocol_GetU16(request->data);
            break;
        case BLE_OPERATION_APPLY_CALIBRATION:
            if (request->data_length != 2U) return false;
            command.id = COMMAND_CALIBRATION_COMMIT;
            command.flags = BleCommandProtocol_GetU16(request->data);
            break;
        case BLE_OPERATION_CANCEL_CALIBRATION:
            if (request->data_length != 2U) return false;
            command.id = COMMAND_CALIBRATION_CANCEL;
            command.flags = BleCommandProtocol_GetU16(request->data);
            break;
        default: return false;
    }
    (void)CommandService_Execute(&command, response);
    *result = MapResult(response->result);
    return true;
}

static void QueueResponse(CachedResponse *entry)
{
    if ((entry != NULL) && entry->valid && (entry->response_length != 0U))
    {
        if (BleTransport_WritePriority(entry->response, entry->response_length))
            ++s_diagnostics.responses_queued;
        else
            ++s_diagnostics.response_queue_full;
    }
}

static void BuildAndQueue(CachedResponse *entry, const BleCommandRequest *request,
                          BleCommandResult result, uint16_t detail,
                          const uint8_t *data, uint16_t data_length)
{
    BleCommandResponse response;
    response.transaction_id = request->transaction_id;
    response.operation = request->operation;
    response.result = result;
    response.detail_code = detail;
    response.data_length = data_length;
    response.data = data;
    if (BleCommandProtocol_EncodeResponse(&response, s_response_sequence++, s_now_ms,
        entry->response, sizeof(entry->response), &entry->response_length))
    {
        entry->pending = false;
        QueueResponse(entry);
        ++s_diagnostics.responses_sent;
    }
}

static void HandleRequest(const BleCommandRequest *request, void *context)
{
    CachedResponse *entry;
    uint8_t data[BLE_COMMAND_MAX_RESPONSE_DATA];
    uint16_t data_length = 0U;
    CommandResponse command_response;
    BleCommandResult result;
    (void)context;
    ++s_diagnostics.requests_received;
    entry = FindCache(request);
    if (entry == (CachedResponse *)(uintptr_t)1U)
    {
        CachedResponse conflict;
        ++s_diagnostics.transaction_conflicts;
        (void)memset(&conflict, 0, sizeof(conflict));
        conflict.valid = true;
        BuildAndQueue(&conflict, request,
            BLE_COMMAND_RESULT_TRANSACTION_CONFLICT, 0U, NULL, 0U);
        return;
    }
    if (entry != NULL)
    {
        ++s_diagnostics.duplicate_requests;
        if (!entry->pending) QueueResponse(entry);
        return;
    }
    entry = AllocateCache();
    if (entry == NULL)
    {
        ++s_diagnostics.response_queue_full;
        return;
    }
    (void)memset(entry, 0, sizeof(*entry));
    entry->valid = true;
    entry->transaction_id = request->transaction_id;
    entry->operation = request->operation;
    entry->request_flags = request->flags;
    entry->request_length = request->data_length;
    if (request->data_length != 0U)
        (void)memcpy(entry->request_data, request->data, request->data_length);
    if ((request->operation == BLE_OPERATION_GET_DEVICE_INFO) &&
        (request->data_length == 0U))
    {
        data_length = BuildDeviceInfo(data);
        result = BLE_COMMAND_RESULT_OK;
        BuildAndQueue(entry, request, result, 0U, data, data_length);
        return;
    }
    if ((request->operation == BLE_OPERATION_GET_ACTIVE_CONFIG) &&
        (request->data_length == 0U))
    {
        data_length = BuildActiveConfig(data);
        result = (data_length != 0U) ? BLE_COMMAND_RESULT_OK :
                                      BLE_COMMAND_RESULT_INVALID_STATE;
        BuildAndQueue(entry, request, result, 0U, data, data_length);
        return;
    }
    if ((request->operation == BLE_OPERATION_GET_CALIBRATION_STATE) &&
        (request->data_length == 0U))
    {
        data_length = BuildCalibrationState(data);
        result = (data_length != 0U) ? BLE_COMMAND_RESULT_OK :
                                      BLE_COMMAND_RESULT_INVALID_STATE;
        BuildAndQueue(entry, request, result, 0U, data, data_length);
        return;
    }
    (void)memset(&command_response, 0, sizeof(command_response));
    if (!IsKnownOperation(request->operation))
    {
        result = BLE_COMMAND_RESULT_INVALID_COMMAND;
        command_response.result = COMMAND_RESULT_INVALID_ARGUMENT;
    }
    else if ((request->operation == BLE_OPERATION_SET_PROFILE_FIELD) &&
             (request->data_length == 10U) && (request->data[1] < 2U))
    {
        result = BLE_COMMAND_RESULT_UNSUPPORTED;
        command_response.result = COMMAND_RESULT_NOT_IMPLEMENTED;
    }
    else if (!ExecuteCommand(request, &command_response, &result))
    {
        result = BLE_COMMAND_RESULT_INVALID_ARGUMENT;
        command_response.result = COMMAND_RESULT_INVALID_ARGUMENT;
    }
    if ((request->operation == BLE_OPERATION_SAVE_CONFIG) &&
        (command_response.result == COMMAND_RESULT_ACCEPTED))
    {
        entry->pending = true;
        s_pending_save_index = (int8_t)(entry - s_cache);
        ++s_diagnostics.pending_save_requests;
        return;
    }
    if (IsCalibrationOperation(request->operation))
        data_length = BuildCalibrationState(data);
    BuildAndQueue(entry, request, result, (uint16_t)command_response.result,
                  (data_length != 0U) ? data : NULL, data_length);
}

void BleCommandService_Init(void)
{
    (void)memset(&s_parser, 0, sizeof(s_parser));
    (void)memset(s_cache, 0, sizeof(s_cache));
    (void)memset(&s_diagnostics, 0, sizeof(s_diagnostics));
    s_cache_next = 0U;
    s_pending_save_index = -1;
    s_response_sequence = 0U;
    s_now_ms = 0U;
    s_initialized = true;
}

void BleCommandService_Process(uint32_t now_ms)
{
    uint8_t byte;
    uint16_t length;
    uint16_t bytes = 0U;
    uint8_t frames = 0U;
    BleCommandParserCounters counters;
    if (!s_initialized) return;
    s_now_ms = now_ms;
    while ((bytes < BLE_COMMAND_RX_BYTES_PER_RUN) &&
           (frames < BLE_COMMAND_FRAMES_PER_RUN))
    {
        if (!BleTransport_Read(&byte, 1U, &length) || (length == 0U)) break;
        ++bytes;
        if (BleCommandParser_Push(&s_parser, byte, HandleRequest, NULL)) ++frames;
    }
    BleCommandParser_GetCounters(&s_parser, &counters);
    s_diagnostics.parser_crc_errors = counters.crc_errors;
    s_diagnostics.parser_length_errors = counters.length_errors;
    s_diagnostics.parser_resync_count = counters.resync_count;
}

void BleCommandService_HandleEvent(const AppEvent *event)
{
    CachedResponse *entry;
    BleCommandRequest request;
    if ((event == NULL) || (s_pending_save_index < 0)) return;
    if ((event->type != EVENT_CONFIG_SAVE_COMPLETED) &&
        (event->type != EVENT_CONFIG_SAVE_NO_CHANGE) &&
        (event->type != EVENT_CONFIG_SAVE_FAILED)) return;
    entry = &s_cache[(uint8_t)s_pending_save_index];
    request.transaction_id = entry->transaction_id;
    request.operation = entry->operation;
    request.flags = 0U;
    request.data_length = entry->request_length;
    request.data = NULL;
    BuildAndQueue(entry, &request,
        (event->type == EVENT_CONFIG_SAVE_FAILED) ?
            BLE_COMMAND_RESULT_PERSISTENCE_FAILED : BLE_COMMAND_RESULT_OK,
        (event->type == EVENT_CONFIG_SAVE_FAILED) ? (uint16_t)event->arg0 : 0U,
        NULL, 0U);
    s_pending_save_index = -1;
}

void BleCommandService_GetDiagnostics(BleCommandServiceDiagnostics *diagnostics)
{
    if (diagnostics != NULL) *diagnostics = s_diagnostics;
}
