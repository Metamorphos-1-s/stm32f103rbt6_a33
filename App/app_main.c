#include "app_main.h"

#include "bsp_board.h"
#include "bsp_time.h"
#include "config_store.h"
#include "event_queue.h"
#include "fault_manager.h"
#include "project_config.h"
#include "scheduler.h"
#include "system_context.h"

#include <stddef.h>
#include <stdint.h>

static void App_1msTask(void *context);
static void App_10msTask(void *context);
static void App_20msTask(void *context);
static void App_100msTask(void *context);
static void App_500msTask(void *context);
static void App_HandleEvent(const AppEvent *event);
static void App_RunStateMachine(void);
static bool App_PushStartupEvent(EventType type);

bool App_Init(void)
{
  DeviceConfig config;
  ConfigStoreResult load_result;
  uint32_t now_ms;

  if (!BSP_BoardInit())
  {
    return false;
  }

  Scheduler_Init();
  EventQueue_Init();
  FaultManager_Init();
  ConfigStore_Init();

  load_result = ConfigStore_Load(&config);
  if (load_result == CONFIG_STORE_NOT_FOUND)
  {
    ConfigStore_LoadDefaults(&config);
  }
  else if (load_result != CONFIG_STORE_OK)
  {
    ConfigStore_LoadDefaults(&config);
    FaultManager_Set(FAULT_CONFIG_INVALID);
  }

  now_ms = BSP_TimeNowMs();
  if (!SystemContext_Init(&config, now_ms))
  {
    return false;
  }

  if ((Scheduler_AddPeriodicTask(App_1msTask, NULL, 1U, 1U) < 0) ||
      (Scheduler_AddPeriodicTask(App_10msTask, NULL, 10U, 10U) < 0) ||
      (Scheduler_AddPeriodicTask(App_20msTask, NULL, 20U, 20U) < 0) ||
      (Scheduler_AddPeriodicTask(App_100msTask, NULL, 100U, 100U) < 0) ||
      (Scheduler_AddPeriodicTask(App_500msTask, NULL, 500U, 500U) < 0))
  {
    FaultManager_Set(FAULT_SCHEDULER_ERROR);
    return false;
  }

  if (!App_PushStartupEvent(EVENT_CONFIG_LOADED) ||
      !App_PushStartupEvent(EVENT_SYSTEM_STARTED))
  {
    FaultManager_Set(FAULT_EVENT_QUEUE_OVERFLOW);
    return false;
  }

  return true;
}

void App_Run(void)
{
  AppEvent event;
  uint8_t processed = 0U;
  static uint32_t observed_dropped_count;

  Scheduler_RunPending();

  while ((processed < APP_MAX_EVENTS_PER_RUN) && EventQueue_Pop(&event))
  {
    App_HandleEvent(&event);
    ++processed;
  }

  if (EventQueue_DroppedCount() != observed_dropped_count)
  {
    observed_dropped_count = EventQueue_DroppedCount();
    FaultManager_Set(FAULT_EVENT_QUEUE_OVERFLOW);
  }

  App_RunStateMachine();
}

static void App_1msTask(void *context)
{
  (void)context;
  BSP_BoardProcess();
}

static void App_10msTask(void *context)
{
  (void)context;
}

static void App_100msTask(void *context)
{
  (void)context;
}

static void App_20msTask(void *context)
{
  (void)context;
}

static void App_500msTask(void *context)
{
  (void)context;
}

static void App_HandleEvent(const AppEvent *event)
{
  if (event == NULL)
  {
    return;
  }

  switch (event->type)
  {
    case EVENT_CONFIG_LOADED:
    case EVENT_SYSTEM_STARTED:
    case EVENT_CONFIG_CHANGED:
    case EVENT_KEY:
    case EVENT_NEW_ADC_SAMPLE:
    case EVENT_NEW_WEIGHT_SAMPLE:
    case EVENT_WEIGHT_STABLE_CHANGED:
    case EVENT_LIMIT_STATE_CHANGED:
    case EVENT_UART_FRAME_RECEIVED:
    case EVENT_BLE_STATE_CHANGED:
    case EVENT_FAULT_RAISED:
    case EVENT_FAULT_CLEARED:
    case EVENT_NONE:
    default:
      break;
  }
}

static void App_RunStateMachine(void)
{
  AppState state = SystemContext_GetState();
  AppState next_state = state;

  if (FaultManager_GetActiveMask() != 0U)
  {
    next_state = APP_STATE_FAULT;
  }
  else
  {
    switch (state)
    {
      case APP_STATE_BOOT:
        next_state = APP_STATE_SELF_TEST;
        break;
      case APP_STATE_SELF_TEST:
        next_state = APP_STATE_LOAD_CONFIG;
        break;
      case APP_STATE_LOAD_CONFIG:
        next_state = APP_STATE_DEVICE_INIT;
        break;
      case APP_STATE_DEVICE_INIT:
        next_state = APP_STATE_WARMUP;
        break;
      case APP_STATE_WARMUP:
        next_state = APP_STATE_RUN;
        break;
      case APP_STATE_RUN:
      case APP_STATE_MENU:
      case APP_STATE_CALIBRATION:
      case APP_STATE_FAULT:
      default:
        break;
    }
  }

  if (next_state != state)
  {
    if (!SystemContext_SetState(next_state, BSP_TimeNowMs()))
    {
      return;
    }
  }
}

static bool App_PushStartupEvent(EventType type)
{
  AppEvent event;

  event.type = type;
  event.timestamp_ms = BSP_TimeNowMs();
  event.arg0 = 0U;
  event.arg1 = 0U;
  event.source = NULL;
  return EventQueue_Push(&event);
}
