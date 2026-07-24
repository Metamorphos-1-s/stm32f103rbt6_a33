#ifndef STAGE5B_TRANSPORT_ADAPTERS_H
#define STAGE5B_TRANSPORT_ADAPTERS_H

#include <stdbool.h>
#include <stdint.h>

void Stage5B_TransportReset(void);
void Stage5B_SetNowUs(uint32_t now_us);
void Stage5B_SetTxComplete(bool complete);
bool Stage5B_IsDeAsserted(void);
void Stage5B_SetDmaWritePosition(uint16_t position);

#endif
