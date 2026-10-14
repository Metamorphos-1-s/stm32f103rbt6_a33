#ifndef MEASUREMENT_BRIDGE_H
#define MEASUREMENT_BRIDGE_H

#include "event_queue.h"
#include "project_config.h"

#include <stdbool.h>
#include <stdint.h>

void MeasurementBridge_Init(void);
uint8_t MeasurementBridge_Process(uint8_t maximum_samples);
uint32_t MeasurementBridge_GetConsumedCount(void);
uint32_t MeasurementBridge_GetInvalidCount(void);
#if (A33_ENABLE_STAGE5P_R5_CHECKWEIGH_INTEGRATION != 0U)
bool MeasurementBridge_RejectedSince(uint32_t *last_seen_count);
#endif
uint16_t MeasurementBridge_GetLastBacklog(void);
uint32_t MeasurementBridge_GetObservedOverrunCount(void);
bool MeasurementBridge_BuildUpdateEvent(uint32_t last_published_count,
                                        AppEvent *event);

#endif /* MEASUREMENT_BRIDGE_H */
