#ifndef BLE_TELEMETRY_SERVICE_H
#define BLE_TELEMETRY_SERVICE_H

#include "ble_telemetry_scheduler.h"

#include <stdint.h>

void BleTelemetryService_Init(uint32_t now_ms);
void BleTelemetryService_Process(uint32_t now_ms);
void BleTelemetryService_GetCounters(BleTelemetryCounters *counters);

#endif
