#ifndef STAGE2A_MOCK_HAL_H
#define STAGE2A_MOCK_HAL_H

#include <stdbool.h>
#include <stdint.h>

void TestMock_Reset(void);
void TestMock_SetTimeMs(uint32_t now_ms);
bool TestMock_IsW02Asserted(void);
uint32_t TestMock_GetEventCount(void);

#endif /* STAGE2A_MOCK_HAL_H */
