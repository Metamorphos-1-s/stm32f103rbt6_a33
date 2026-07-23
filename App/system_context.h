#ifndef SYSTEM_CONTEXT_H
#define SYSTEM_CONTEXT_H

#include "app_state.h"
#include "device_config.h"
#include "runtime_state.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
  DeviceConfig config;
  RuntimeState runtime;
  AppState state;
  uint32_t state_enter_time_ms;
  bool initialized;
} SystemContext;

bool SystemContext_Init(const DeviceConfig *config, uint32_t now_ms);
const SystemContext *SystemContext_Get(void);
AppState SystemContext_GetState(void);
bool SystemContext_SetState(AppState state, uint32_t now_ms);
bool SystemContext_SetTareState(int32_t tare_weight, bool tare_active);
bool SystemContext_SetConfigDirty(bool dirty);
bool SystemContext_SetWeightView(WeightViewMode view);

#endif /* SYSTEM_CONTEXT_H */
