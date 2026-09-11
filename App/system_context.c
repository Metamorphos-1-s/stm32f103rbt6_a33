#include "system_context.h"
#include "persistent_codec.h"
#include "revision_helper.h"

#include <stddef.h>
#include <limits.h>
#include <string.h>

static SystemContext s_system_context;

bool SystemContext_Init(const DeviceConfig *config, uint32_t now_ms)
{
  RuntimeState runtime;

  if (config == NULL)
  {
    return false;
  }

  (void)memset(&runtime, 0, sizeof(runtime));
  runtime.weight_view = WEIGHT_VIEW_NET;
  return SystemContext_InitRestored(config, &runtime, 0U, false, now_ms);
}

bool SystemContext_InitRestored(const DeviceConfig *config,
                                const RuntimeState *runtime,
                                uint32_t stored_revision,
                                bool storage_has_record,
                                uint32_t now_ms)
{
  if ((config == NULL) || (runtime == NULL))
  {
    return false;
  }

  (void)memset(&s_system_context, 0, sizeof(s_system_context));
  s_system_context.config = *config;
  s_system_context.runtime = *runtime;
  if ((uint32_t)s_system_context.config.display.default_weight_view <
      (uint32_t)WEIGHT_VIEW_COUNT)
  {
    s_system_context.runtime.weight_view =
        (WeightViewMode)s_system_context.config.display.default_weight_view;
  }
  s_system_context.runtime.config_dirty = false;
  s_system_context.config_revision = stored_revision;
  s_system_context.saved_revision = stored_revision;
  s_system_context.runtime.boot_count += 1U;
  s_system_context.state = APP_STATE_BOOT;
  s_system_context.state_enter_time_ms = now_ms;
  s_system_context.storage_has_record = storage_has_record;
  s_system_context.initialized = true;
  return true;
}

const SystemContext *SystemContext_Get(void)
{
  return s_system_context.initialized ? &s_system_context : NULL;
}

AppState SystemContext_GetState(void)
{
  return s_system_context.state;
}

bool SystemContext_SetState(AppState state, uint32_t now_ms)
{
  if (!s_system_context.initialized)
  {
    return false;
  }

  s_system_context.state = state;
  s_system_context.state_enter_time_ms = now_ms;
  return true;
}

bool SystemContext_SetTareState(int32_t tare_weight, bool tare_active)
{
  return SystemContext_SetTareStateMass((MassValueUg)tare_weight, tare_active);
}

bool SystemContext_SetTareStateMass(MassValueUg tare_mass_ug, bool tare_active)
{
  if (!s_system_context.initialized)
  {
    return false;
  }
  int32_t tare_compat;
  tare_mass_ug = tare_active ? tare_mass_ug : 0;
  tare_compat = (tare_mass_ug > INT32_MAX) ? INT32_MAX :
      ((tare_mass_ug < INT32_MIN) ? INT32_MIN : (int32_t)tare_mass_ug);
  if ((s_system_context.runtime.current_tare_ug != tare_mass_ug) ||
      (s_system_context.runtime.tare_active != tare_active))
  {
    s_system_context.runtime.current_tare_ug = tare_mass_ug;
    s_system_context.runtime.current_tare = tare_compat;
    s_system_context.runtime.tare_active = tare_active;
    (void)SystemContext_MarkConfigChanged();
  }
  return true;
}

bool SystemContext_SyncTareStateMass(MassValueUg tare_mass_ug, bool tare_active)
{
  int32_t tare_compat;
  if (!s_system_context.initialized) return false;
  tare_mass_ug = tare_active ? tare_mass_ug : 0;
  tare_compat = (tare_mass_ug > INT32_MAX) ? INT32_MAX :
      ((tare_mass_ug < INT32_MIN) ? INT32_MIN : (int32_t)tare_mass_ug);
  s_system_context.runtime.current_tare_ug = tare_mass_ug;
  s_system_context.runtime.current_tare = tare_compat;
  s_system_context.runtime.tare_active = tare_active;
  return true;
}

bool SystemContext_SetConfigDirty(bool dirty)
{
  if (!s_system_context.initialized)
  {
    return false;
  }
  s_system_context.runtime.config_dirty = dirty;
  return true;
}

bool SystemContext_SetWeightView(WeightViewMode view)
{
  if (!s_system_context.initialized ||
      ((uint32_t)view >= (uint32_t)WEIGHT_VIEW_COUNT))
  {
    return false;
  }
  if (s_system_context.runtime.weight_view != view)
  {
    s_system_context.runtime.weight_view = view;
    (void)SystemContext_MarkConfigChanged();
  }
  return true;
}

bool SystemContext_SetRuntimeWeightView(WeightViewMode view)
{
  if (!s_system_context.initialized ||
      ((uint32_t)view >= (uint32_t)WEIGHT_VIEW_COUNT))
  {
    return false;
  }
  s_system_context.runtime.weight_view = view;
  return true;
}

bool SystemContext_ApplyConfig(const DeviceConfig *config, bool dirty)
{
  if (!s_system_context.initialized || (config == NULL))
  {
    return false;
  }
  if (!PersistentCodec_DeviceConfigEqual(&s_system_context.config, config))
  {
    s_system_context.config = *config;
    if (dirty)
    {
      (void)SystemContext_MarkConfigChanged();
    }
  }
  else if (dirty)
  {
    s_system_context.runtime.config_dirty =
        s_system_context.config_revision != s_system_context.saved_revision;
  }
  return true;
}

bool SystemContext_ReplaceConfig(const DeviceConfig *config, bool dirty)
{
  if (!s_system_context.initialized || (config == NULL)) return false;
  s_system_context.config = *config;
  s_system_context.runtime.config_dirty = dirty;
  return true;
}

bool SystemContext_FinalizeSavedRevision(uint32_t revision)
{
  if (!s_system_context.initialized || !Revision_IsValid(revision)) return false;
  s_system_context.config_revision = revision;
  s_system_context.saved_revision = revision;
  s_system_context.runtime.config_dirty = false;
  s_system_context.storage_has_record = true;
  return true;
}

bool SystemContext_RestoreSnapshot(const DeviceConfig *config,
    const RuntimeState *runtime, uint32_t current_revision,
    uint32_t saved_revision)
{
  if (!s_system_context.initialized || (config == NULL) || (runtime == NULL) ||
      (current_revision == 0xFFFFFFFFUL) ||
      (saved_revision == 0xFFFFFFFFUL)) return false;
  s_system_context.config = *config;
  s_system_context.runtime = *runtime;
  s_system_context.config_revision = current_revision;
  s_system_context.saved_revision = saved_revision;
  return true;
}

uint32_t SystemContext_GetConfigRevision(void)
{
  return s_system_context.config_revision;
}

uint32_t SystemContext_GetSavedRevision(void)
{
  return s_system_context.saved_revision;
}

bool SystemContext_MarkConfigChanged(void)
{
  uint32_t next;
  if (!s_system_context.initialized)
  {
    return false;
  }
  next = Revision_Next(s_system_context.config_revision);
  s_system_context.config_revision = next;
  s_system_context.runtime.config_dirty =
      next != s_system_context.saved_revision;
  return true;
}

bool SystemContext_MarkRevisionSaved(uint32_t revision)
{
  if (!s_system_context.initialized || !Revision_IsValid(revision))
  {
    return false;
  }
  s_system_context.saved_revision = revision;
  s_system_context.storage_has_record = true;
  s_system_context.runtime.config_dirty =
      s_system_context.config_revision != s_system_context.saved_revision;
  return true;
}

bool SystemContext_HasStorageRecord(void)
{
  return s_system_context.storage_has_record;
}
