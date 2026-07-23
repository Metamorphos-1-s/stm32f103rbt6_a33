#ifndef CS1237_H
#define CS1237_H

#include "cs1237_types.h"

#include <stdbool.h>
#include <stdint.h>

bool CS1237_Init(const CS1237_Config *config);
void CS1237_Process(void);
bool CS1237_IsReady(void);
bool CS1237_TryPopSample(CS1237_Sample *sample);
uint16_t CS1237_GetBufferedSampleCount(void);
uint32_t CS1237_GetSampleCount(void);
uint32_t CS1237_GetBufferOverrunCount(void);
uint32_t CS1237_GetReadErrorCount(void);
CS1237_State CS1237_GetState(void);

bool CS1237_WriteConfig(const CS1237_Config *config);
bool CS1237_ReadConfig(CS1237_Config *config);
bool CS1237_VerifyConfig(const CS1237_Config *expected);
uint8_t CS1237_GetLastConfigRegister(void);
void CS1237_EnterSafeState(void);

int32_t CS1237_SignExtend24(uint32_t raw24);
bool CS1237_EncodeConfig(const CS1237_Config *config, uint8_t *register_value);
bool CS1237_DecodeConfig(uint8_t register_value, CS1237_Config *config);

#if defined(STAGE2A_HOST_TEST)
bool CS1237_TestPushSample(const CS1237_Sample *sample);
#endif

#endif /* CS1237_H */
