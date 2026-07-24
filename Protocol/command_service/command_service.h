#ifndef COMMAND_SERVICE_H
#define COMMAND_SERVICE_H

#include "command_types.h"
#include "device_config.h"

void CommandService_Init(void);
CommandResult CommandService_Execute(const CommandRequest *request,
                                     CommandResponse *response);
const CalibrationConfig *CommandService_GetCalibrationCandidate(void);
bool CommandService_SetStagedConfig(const DeviceConfig *candidate);
void CommandService_ClearStagedConfig(void);

#endif /* COMMAND_SERVICE_H */
