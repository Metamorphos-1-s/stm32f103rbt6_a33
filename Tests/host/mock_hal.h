#ifndef STAGE2A_MOCK_HAL_H
#define STAGE2A_MOCK_HAL_H

#include <stdbool.h>
#include <stdint.h>

#include "event_queue.h"
#include "output_gpio.h"

void TestMock_Reset(void);
void TestMock_SetTimeMs(uint32_t now_ms);
bool TestMock_IsW02Asserted(void);
uint32_t TestMock_GetEventCount(void);
uint32_t TestMock_GetEventTypeCount(EventType type);
bool TestMock_IsOutputEnabled(OutputId output);

#endif /* STAGE2A_MOCK_HAL_H */
