#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "device_config.h"

#include <stdbool.h>
#include <stdint.h>

#define DEVICE_ERROR_CS1237_ENABLE_UNCONFIRMED  (1UL << 0U)
#define DEVICE_ERROR_CS1237_CONFIG              (1UL << 1U)
#define DEVICE_ERROR_CS1237_BUFFER              (1UL << 2U)
#define DEVICE_ERROR_TM1628                     (1UL << 3U)
#define DEVICE_ERROR_BATTERY_ADC                (1UL << 4U)
#define DEVICE_ERROR_W02_PWRKEY                 (1UL << 5U)

bool DeviceManager_Init(const DeviceConfig *config);
void DeviceManager_ProcessFast(void);
void DeviceManager_Process1ms(void);
void DeviceManager_Process10ms(void);
void DeviceManager_Process20ms(void);
void DeviceManager_Process500ms(void);
bool DeviceManager_IsReady(void);
uint32_t DeviceManager_GetErrorMask(void);
void DeviceManager_ObserveCs1237Consumption(uint32_t consumed_count,
                                            uint16_t backlog,
                                            uint32_t overrun_count);
uint16_t DeviceManager_GetOverrunBacklog(void);
uint32_t DeviceManager_GetOverrunConsumedCount(void);
uint32_t DeviceManager_GetRecordedOverrunCount(void);
void DeviceManager_EnterSafeState(void);
uint8_t DeviceManager_GetLastRawKeyMask(void);
bool DeviceManager_EnterStorageMaintenance(void);
bool DeviceManager_ExitStorageMaintenance(void);
bool DeviceManager_IsInStorageMaintenance(void);

#endif /* DEVICE_MANAGER_H */
