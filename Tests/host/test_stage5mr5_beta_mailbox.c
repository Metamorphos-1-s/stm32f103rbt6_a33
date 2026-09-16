#include "command_service.h"
#include "communication_manager.h"
#include "modbus_command_mailbox.h"
#include "modbus_register_map.h"
#include "reference_lock_drift_compensator.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

static unsigned s_execute_count;
static CommandRequest s_last_request;

CommandResult CommandService_Execute(const CommandRequest *request,
                                     CommandResponse *response)
{
    ++s_execute_count;
    s_last_request = *request;
    (void)memset(response, 0, sizeof(*response));
    response->result = COMMAND_RESULT_OK;
    response->value0 = 123;
    return response->result;
}

void CommunicationManager_BindDeferredSaveToken(CommandSource source,
                                                  uint16_t token)
{
    (void)source;
    (void)token;
}

static int Execute(uint16_t token, uint16_t wire_command, uint16_t argument)
{
    CHECK(ModbusCommandMailbox_Write(0x0040U, token,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusCommandMailbox_Write(0x0041U, wire_command,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusCommandMailbox_Write(0x0042U, 0U,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusCommandMailbox_Write(0x0043U, argument,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusCommandMailbox_Write(0x004BU, MODBUS_EXECUTE_VALUE,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    return 0;
}

int main(void)
{
    uint16_t value;
    ModbusCommandMailbox_Init();
    ModbusCommandMailbox_Process(100U);
    CHECK(Execute(1U, 29U, R5_DRIFT_MODE_STATIC_COMPENSATION) == 0);
    CHECK(s_execute_count == 1U);
    CHECK(s_last_request.id == COMMAND_R5_SET_MODE);
    CHECK(s_last_request.value0 == R5_DRIFT_MODE_STATIC_COMPENSATION);
    CHECK(Execute(1U, 29U, R5_DRIFT_MODE_STATIC_COMPENSATION) == 0);
    CHECK(s_execute_count == 1U);
    CHECK(ModbusCommandMailbox_Read(0x004CU, &value) == MODBUS_REGISTER_OK);
    CHECK(value == 1U);
    CHECK(Execute(2U, 31U, 0U) == 0);
    CHECK(s_execute_count == 2U);
    CHECK(s_last_request.id == COMMAND_R5_GET_STATUS);
    CHECK(Execute(3U, 32U, 1U) == 0);
    CHECK(s_last_request.id == COMMAND_R5_SET_APPLICATION);
    return 0;
}
