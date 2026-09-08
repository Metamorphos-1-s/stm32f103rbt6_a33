#ifndef STAGE2A_MOCK_HAL_H
#define STAGE2A_MOCK_HAL_H

#include <stdbool.h>
#include <stdint.h>

#include "event_queue.h"
#include "output_gpio.h"
#include "persistence_manager.h"
#include "communication_manager.h"

void TestMock_Reset(void);
void TestMock_SetTimeMs(uint32_t now_ms);
bool TestMock_IsW02Asserted(void);
uint32_t TestMock_GetEventCount(void);
uint32_t TestMock_GetEventTypeCount(EventType type);
bool TestMock_IsOutputEnabled(OutputId output);
void TestMock_RejectNextEvents(uint32_t count);
void TestMock_RejectEventTypeOnce(EventType type);
void TestMock_SetPersistenceResult(CommandResult request,
                                   PersistenceStatus status);
void TestMock_SetCommunicationApplyResult(CommandResult request,
                                          CommunicationApplyResult status);
uint32_t TestMock_GetSaveRequestCount(void);
uint32_t TestMock_GetLocalCommunicationApplyCount(void);
void TestMock_SetPersistenceBusy(bool busy);

#endif /* STAGE2A_MOCK_HAL_H */
