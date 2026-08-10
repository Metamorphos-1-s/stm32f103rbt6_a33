#ifndef COMMAND_SERVICE_H
#define COMMAND_SERVICE_H

#include "command_types.h"
#include "device_config.h"

typedef enum
{
    CAL_OWNER_NONE = 0,
    CAL_OWNER_LOCAL_UI,
    CAL_OWNER_MODBUS,
    CAL_OWNER_BLE
} CalibrationOwner;

typedef enum
{
    CAL_WORKFLOW_IDLE = 0,
    CAL_WORKFLOW_WAIT_ZERO_STABLE,
    CAL_WORKFLOW_ZERO_READY,
    CAL_WORKFLOW_ZERO_CAPTURED,
    CAL_WORKFLOW_WAIT_LOAD_STABLE,
    CAL_WORKFLOW_LOAD_READY,
    CAL_WORKFLOW_RESULT_READY,
    CAL_WORKFLOW_APPLIED,
    CAL_WORKFLOW_FAILED
} CalibrationWorkflowState;

typedef struct
{
    CalibrationWorkflowState state;
    CalibrationOwner owner;
    uint16_t session_id;
    MassUnit locked_unit;
    uint8_t locked_decimal_places;
    uint8_t locked_division_digit;
    MassValueUg locked_capacity_ug;
    MassValueUg calibration_mass_ug;
    int32_t zero_raw;
    int32_t load_raw;
    int32_t span_raw;
    uint32_t sample_sequence;
    uint8_t sample_count;
    bool active;
    bool stable;
    bool zero_captured;
    bool load_captured;
    bool candidate_valid;
    bool result_valid;
    bool persistent_dirty;
    bool active_calibration_valid;
    CommandResult last_result;
} CalibrationSessionSnapshot;

void CommandService_Init(void);
void CommandService_Process(uint32_t now_ms);
CommandResult CommandService_Execute(const CommandRequest *request,
                                     CommandResponse *response);
const CalibrationConfig *CommandService_GetCalibrationCandidate(void);
bool CommandService_GetCalibrationSnapshot(
    CalibrationSessionSnapshot *snapshot);
bool CommandService_SetStagedConfig(const DeviceConfig *candidate);
CommandResult CommandService_ReserveConfigOwner(CommandSource source);
void CommandService_ClearStagedConfig(void);

#endif /* COMMAND_SERVICE_H */
