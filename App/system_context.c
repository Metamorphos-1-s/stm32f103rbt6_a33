#include "system_context.h"

#include <stddef.h>
#include <string.h>

static SystemContext s_system_context;

bool SystemContext_Init(const DeviceConfig *config, uint32_t now_ms)
{
  if (config == NULL)
  {
    return false;
  }

  (void)memset(&s_system_context, 0, sizeof(s_system_context));
  s_system_context.config = *config;
  s_system_context.runtime.weight_view = WEIGHT_VIEW_NET;
  s_system_context.runtime.boot_count = 1U;
  s_system_context.state = APP_STATE_BOOT;
  s_system_context.state_enter_time_ms = now_ms;
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
