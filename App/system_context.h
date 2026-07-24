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
  uint32_t config_revision;
  uint32_t saved_revision;
  bool storage_has_record;
  bool initialized;
} SystemContext;

bool SystemContext_Init(const DeviceConfig *config, uint32_t now_ms);
bool SystemContext_InitRestored(const DeviceConfig *config,
                                const RuntimeState *runtime,
                                uint32_t stored_revision,
                                bool storage_has_record,
                                uint32_t now_ms);
const SystemContext *SystemContext_Get(void);
AppState SystemContext_GetState(void);
bool SystemContext_SetState(AppState state, uint32_t now_ms);
bool SystemContext_SetTareState(int32_t tare_weight, bool tare_active);
bool SystemContext_SetConfigDirty(bool dirty);
bool SystemContext_SetWeightView(WeightViewMode view);
bool SystemContext_ApplyConfig(const DeviceConfig *config, bool dirty);
uint32_t SystemContext_GetConfigRevision(void);
uint32_t SystemContext_GetSavedRevision(void);
bool SystemContext_MarkConfigChanged(void);
bool SystemContext_MarkRevisionSaved(uint32_t revision);
bool SystemContext_HasStorageRecord(void);

#endif /* SYSTEM_CONTEXT_H */
