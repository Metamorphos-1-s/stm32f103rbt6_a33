#ifndef STAGE5B_TRANSPORT_ADAPTERS_H
#define STAGE5B_TRANSPORT_ADAPTERS_H

#include <stdbool.h>
#include <stdint.h>
#include "command_types.h"

void Stage5B_TransportReset(void);
void Stage5B_SetNowUs(uint32_t now_us);
void Stage5B_SetTxComplete(bool complete);
bool Stage5B_IsDeAsserted(void);
void Stage5B_SetDmaWritePosition(uint16_t position);
void Stage5B_SetPersistenceBusy(bool busy);
void Stage5B_SetPersistenceSaveResult(CommandResult result);
void Stage5B_SetApplyConfigResult(bool result);

#endif
