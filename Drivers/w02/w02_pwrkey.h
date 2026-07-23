#ifndef W02_PWRKEY_H
#define W02_PWRKEY_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    W02_PWRKEY_IDLE = 0,
    W02_PWRKEY_ASSERTED,
    W02_PWRKEY_ERROR
} W02PwrKeyState;

void W02PwrKey_Init(void);
bool W02PwrKey_RequestPulse(uint32_t low_time_ms);
void W02PwrKey_Process(void);
bool W02PwrKey_IsBusy(void);
W02PwrKeyState W02PwrKey_GetState(void);
bool W02PwrKey_IsPulseDurationValid(uint32_t low_time_ms);

#endif /* W02_PWRKEY_H */
