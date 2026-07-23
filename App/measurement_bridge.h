#ifndef MEASUREMENT_BRIDGE_H
#define MEASUREMENT_BRIDGE_H

#include "event_queue.h"

#include <stdbool.h>
#include <stdint.h>

void MeasurementBridge_Init(void);
uint8_t MeasurementBridge_Process(uint8_t maximum_samples);
uint32_t MeasurementBridge_GetConsumedCount(void);
uint32_t MeasurementBridge_GetInvalidCount(void);
uint16_t MeasurementBridge_GetLastBacklog(void);
uint32_t MeasurementBridge_GetObservedOverrunCount(void);
bool MeasurementBridge_BuildUpdateEvent(uint32_t last_published_count,
                                        AppEvent *event);

#endif /* MEASUREMENT_BRIDGE_H */
