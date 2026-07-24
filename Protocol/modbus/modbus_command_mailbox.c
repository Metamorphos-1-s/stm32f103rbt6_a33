#include "modbus_command_mailbox.h"

#include "command_service.h"
#include "modbus_register_map.h"

#include <stdbool.h>
#include <string.h>

typedef struct
{
    uint16_t request_token;
    uint16_t command_id;
    uint16_t argument0[2];
    uint16_t argument1[2];
    uint16_t argument64[4];
    uint16_t command_flags;
    uint16_t response_token;
    uint16_t command_result;
    uint16_t command_state;
    uint16_t last_command_id;
    uint16_t response_value0[2];
    uint16_t response_value64[4];
    uint16_t response_status[2];
    bool have_response;
} Mailbox;

static Mailbox s_mailbox;

static int32_t Join32(const uint16_t words[2])
{
    return (int32_t)(((uint32_t)words[0] << 16U) | words[1]);
}

static bool MapCommand(uint16_t id, CommandId *command)
{
    static const CommandId map[] = {
        COMMAND_COUNT, COMMAND_ZERO, COMMAND_RESET_ZERO, COMMAND_TARE,
        COMMAND_CLEAR_TARE, COMMAND_SET_WEIGHT_VIEW, COMMAND_SET_DISPLAY_UNIT,
        COMMAND_SWITCH_WEIGHING_PROFILE, COMMAND_REQUEST_MANUAL_OUTPUT,
        COMMAND_BEGIN_CONFIG_EDIT, COMMAND_CONFIG_VALIDATE,
        COMMAND_COMMIT_CONFIG_EDIT, COMMAND_CANCEL_CONFIG_EDIT,
        COMMAND_REQUEST_CONFIG_SAVE, COMMAND_CALIBRATION_BEGIN,
        COMMAND_CALIBRATION_CAPTURE_ZERO, COMMAND_CALIBRATION_SET_SPAN_MASS,
        COMMAND_CALIBRATION_CAPTURE_SPAN, COMMAND_COUNT,
        COMMAND_CALIBRATION_COMMIT, COMMAND_CALIBRATION_CANCEL,
        COMMAND_FACTORY_RESET_REQUEST, COMMAND_FACTORY_RESET_CONFIRM,
        COMMAND_FACTORY_RESET_CANCEL, COMMAND_COMMUNICATION_APPLY
    };
    if (id >= (uint16_t)(sizeof(map) / sizeof(map[0]))) return false;
    *command = map[id];
    return true;
}

static ModbusRegisterResult Execute(void)
{
    CommandRequest request;
    CommandResponse response;
    CommandId command;
    if (s_mailbox.request_token == 0U) return MODBUS_REGISTER_ILLEGAL_VALUE;
    if (s_mailbox.have_response &&
        (s_mailbox.request_token == s_mailbox.response_token))
        return MODBUS_REGISTER_OK;
    if (!MapCommand(s_mailbox.command_id, &command))
        return MODBUS_REGISTER_ILLEGAL_VALUE;
    if (command == COMMAND_COUNT)
    {
        (void)memset(&response, 0, sizeof(response));
        response.result = (s_mailbox.command_id == 0U) ? COMMAND_RESULT_OK :
            COMMAND_RESULT_NOT_IMPLEMENTED;
    }
    else
    {
        request.id = command; request.source = COMMAND_SOURCE_MODBUS;
        request.value0 = Join32(s_mailbox.argument0);
        request.value1 = Join32(s_mailbox.argument1);
        request.flags = s_mailbox.command_flags;
        request.value64 = ((int64_t)(uint64_t)s_mailbox.argument64[0] << 48U) |
            ((int64_t)(uint64_t)s_mailbox.argument64[1] << 32U) |
            ((int64_t)(uint64_t)s_mailbox.argument64[2] << 16U) |
            s_mailbox.argument64[3];
        (void)CommandService_Execute(&request, &response);
    }
    s_mailbox.response_token = s_mailbox.request_token;
    s_mailbox.command_result = (uint16_t)response.result;
    s_mailbox.command_state = 0U;
    s_mailbox.last_command_id = s_mailbox.command_id;
    s_mailbox.response_value0[0] = (uint16_t)((uint32_t)response.value0 >> 16U);
    s_mailbox.response_value0[1] = (uint16_t)response.value0;
    s_mailbox.response_status[0] = (uint16_t)response.status_flags;
    s_mailbox.response_status[1] = (uint16_t)(response.status_flags >> 16U);
    s_mailbox.have_response = true;
    return MODBUS_REGISTER_OK;
}

void ModbusCommandMailbox_Init(void) { (void)memset(&s_mailbox, 0, sizeof(s_mailbox)); }

ModbusRegisterResult ModbusCommandMailbox_Read(uint16_t address, uint16_t *value)
{
    if ((value == NULL) || (address < MODBUS_MAILBOX_FIRST) ||
        (address > MODBUS_MAILBOX_LAST)) return MODBUS_REGISTER_ILLEGAL_ADDRESS;
    *value = 0U;
    switch (address)
    {
        case 0x0040U: *value=s_mailbox.request_token; break;
        case 0x0041U: *value=s_mailbox.command_id; break;
        case 0x0042U: case 0x0043U: *value=s_mailbox.argument0[address-0x0042U]; break;
        case 0x0044U: case 0x0045U: *value=s_mailbox.argument1[address-0x0044U]; break;
        case 0x0046U: case 0x0047U: case 0x0048U: case 0x0049U:
            *value=s_mailbox.argument64[address-0x0046U]; break;
        case 0x004AU: *value=s_mailbox.command_flags; break;
        case 0x004BU: *value=0U; break;
        case 0x004CU: *value=s_mailbox.response_token; break;
        case 0x004DU: *value=s_mailbox.command_result; break;
        case 0x004EU: *value=s_mailbox.command_state; break;
        case 0x004FU: *value=s_mailbox.last_command_id; break;
        case 0x0050U: case 0x0051U: *value=s_mailbox.response_value0[address-0x0050U]; break;
        case 0x0052U: case 0x0053U: case 0x0054U: case 0x0055U:
            *value=s_mailbox.response_value64[address-0x0052U]; break;
        case 0x0056U: case 0x0057U: *value=s_mailbox.response_status[address-0x0056U]; break;
        default: break;
    }
    return MODBUS_REGISTER_OK;
}

ModbusRegisterResult ModbusCommandMailbox_Write(uint16_t address, uint16_t value)
{
    if ((address < 0x0040U) || (address > 0x004BU))
        return (address <= MODBUS_MAILBOX_LAST) ? MODBUS_REGISTER_READ_ONLY :
            MODBUS_REGISTER_ILLEGAL_ADDRESS;
    switch (address)
    {
        case 0x0040U: s_mailbox.request_token=value; break;
        case 0x0041U: s_mailbox.command_id=value; break;
        case 0x0042U: case 0x0043U: s_mailbox.argument0[address-0x0042U]=value; break;
        case 0x0044U: case 0x0045U: s_mailbox.argument1[address-0x0044U]=value; break;
        case 0x0046U: case 0x0047U: case 0x0048U: case 0x0049U:
            s_mailbox.argument64[address-0x0046U]=value; break;
        case 0x004AU: s_mailbox.command_flags=value; break;
        case 0x004BU:
            if (value != MODBUS_EXECUTE_VALUE) return MODBUS_REGISTER_ILLEGAL_VALUE;
            return Execute();
        default: return MODBUS_REGISTER_ILLEGAL_ADDRESS;
    }
    return MODBUS_REGISTER_OK;
}

uint16_t ModbusCommandMailbox_GetLastResult(void) { return s_mailbox.command_result; }
