#ifndef FAULT_MANAGER_H
#define FAULT_MANAGER_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  FAULT_NONE = 0,
  FAULT_CONFIG_INVALID,
  FAULT_EVENT_QUEUE_OVERFLOW,
  FAULT_SCHEDULER_ERROR,
  FAULT_ADC_ERROR,
  FAULT_CS1237_NOT_READY,
  FAULT_CS1237_DATA_ERROR,
  FAULT_CALIBRATION_INVALID,
  FAULT_UART_ERROR,
  FAULT_BATTERY_OVERVOLTAGE,
  FAULT_BATTERY_LOW
} FaultCode;

void FaultManager_Init(void);
void FaultManager_Set(FaultCode fault);
void FaultManager_Clear(FaultCode fault);
bool FaultManager_IsActive(FaultCode code);
uint32_t FaultManager_GetActiveMask(void);

#endif /* FAULT_MANAGER_H */
