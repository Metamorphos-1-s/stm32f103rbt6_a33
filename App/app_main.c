#include "app_main.h"

#include "bsp_board.h"
#include "bsp_flash.h"
#include "bsp_time.h"
#include "calibration_controller.h"
#include "command_service.h"
#include "config_store.h"
#include "default_config.h"
#include "device_manager.h"
#include "display_codes.h"
#include "display_controller.h"
#include "event_queue.h"
#include "fault_manager.h"
#include "measurement_bridge.h"
#include "key_service.h"
#include "menu_controller.h"
#include "metrology_manager.h"
#include "project_config.h"
#include "persistence_manager.h"
#include "raw_measurement.h"
#include "scheduler.h"
#include "self_test_controller.h"
#include "stage2b_board_diagnostics.h"
#include "stage3_metrology_diagnostics.h"
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
static bool App_PushEvent(EventType type, uint32_t arg0, uint32_t arg1);
static void App_PublishRawMeasurement(void);
static void App_ProcessKeyEvent(const KeyEvent *event);
static CommandResult App_ExecuteLocalCommand(CommandId id, int32_t value0);
static void App_ShowCommandResult(CommandResult result, bool tare_action);

static bool s_device_manager_init_attempted;
static bool s_fault_entry_applied;
static uint32_t s_last_published_raw_count;

bool App_Init(void)
{
  DeviceConfig config;
  RuntimeState runtime = {0};
  ConfigLoadResult load_result;
  const ConfigLoadInfo *load_info;
  bool storage_has_record;
  uint32_t now_ms;

  if (!BSP_BoardInit())
  {
    return false;
  }

  Scheduler_Init();
  EventQueue_Init();
  FaultManager_Init();
  if (!BSP_FlashValidateLayout())
  {
    FaultManager_Set(FAULT_CONFIG_FLASH_LAYOUT);
  }
  ConfigStore_Init(BSP_FlashGetBackend());
  (void)PersistenceManager_Init();
  MeasurementBridge_Init();
  s_device_manager_init_attempted = false;
  s_fault_entry_applied = false;
  s_last_published_raw_count = 0U;

  load_result = PersistenceManager_LoadStartup(&config, &runtime);
  storage_has_record = (load_result == CONFIG_LOAD_OK) ||
      (load_result == CONFIG_LOAD_RECOVERED_SLOT_A) ||
      (load_result == CONFIG_LOAD_RECOVERED_SLOT_B) ||
      (load_result == CONFIG_LOAD_BOTH_VALID);
  if (!storage_has_record)
  {
    DefaultConfig_Load(&config);
    runtime.weight_view = WEIGHT_VIEW_NET;
    if (load_result == CONFIG_LOAD_IO_ERROR)
    {
      FaultManager_Set(FAULT_CONFIG_FLASH_IO);
    }
  }
  load_info = PersistenceManager_GetLoadInfo();

  now_ms = BSP_TimeNowMs();
  if (!SystemContext_InitRestored(&config, &runtime,
      storage_has_record ? load_info->active_sequence : 0U,
      storage_has_record, now_ms))
  {
    return false;
  }
  (void)MetrologyManager_Init(&config, &SystemContext_Get()->runtime);
  CommandService_Init();
  MenuController_Init();
  SelfTestController_Init();
  if (!KeyService_Init(&g_key_map_development_default))
  {
    FaultManager_Set(FAULT_UI_KEY_MAP_INVALID);
  }
  if (!DisplayController_Init())
  {
    FaultManager_Set(FAULT_UI_DISPLAY_ERROR);
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
  if (((load_result == CONFIG_LOAD_RECOVERED_SLOT_A) ||
       (load_result == CONFIG_LOAD_RECOVERED_SLOT_B)) &&
      !App_PushEvent(EVENT_CONFIG_LOAD_RECOVERED,
                     load_info->active_slot, load_info->active_sequence))
  {
    FaultManager_Set(FAULT_EVENT_QUEUE_OVERFLOW);
    return false;
  }
  if (!storage_has_record &&
      !App_PushEvent(EVENT_CONFIG_LOAD_DEFAULTS, (uint32_t)load_result, 0U))
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

  DeviceManager_ProcessFast();
  if (!DeviceManager_IsInStorageMaintenance())
  {
    (void)MeasurementBridge_Process(
        MEASUREMENT_BRIDGE_MAX_SAMPLES_PER_RUN);
  }
  DeviceManager_ObserveCs1237Consumption(
      MeasurementBridge_GetConsumedCount(),
      MeasurementBridge_GetLastBacklog(),
      MeasurementBridge_GetObservedOverrunCount());
  Stage2B_DiagnosticsUpdateCs1237Stats(
      MeasurementBridge_GetLastBacklog(),
      MeasurementBridge_GetObservedOverrunCount());
  Scheduler_RunPending();
  PersistenceManager_Process();

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

  Stage2B_DiagnosticsProcess();
  App_RunStateMachine();
}

bool App_ExitDiagnostics(void)
{
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
  if (SystemContext_GetState() != APP_STATE_DIAGNOSTIC)
  {
    return false;
  }

  Stage2B_DiagnosticsStop();
  (void)DisplayController_Init();
  return SystemContext_SetState(APP_STATE_RUN, BSP_TimeNowMs());
#else
  return false;
#endif
}

static void App_1msTask(void *context)
{
  (void)context;
  BSP_BoardProcess();
  DeviceManager_Process1ms();
}

static void App_10msTask(void *context)
{
  KeyEvent event;
  uint8_t processed = 0U;

  (void)context;
  DeviceManager_Process10ms();
  if (SystemContext_GetState() != APP_STATE_DIAGNOSTIC)
  {
    KeyService_Process10ms(DeviceManager_GetLastRawKeyMask(), BSP_TimeNowMs());
    while ((processed < KEY_EVENTS_PER_TICK_MAX) &&
           KeyService_TryPopEvent(&event))
    {
      App_ProcessKeyEvent(&event);
      ++processed;
    }
  }
  SelfTestController_Process10ms();
  MenuController_Process10ms();
  CalibrationController_Process10ms();

  if (MenuController_TakeCalibrationRequest())
  {
    if (PersistenceManager_IsBusy())
    {
      (void)MenuController_Enter();
    }
    else if (CalibrationController_Begin())
    {
      (void)SystemContext_SetState(APP_STATE_CALIBRATION, BSP_TimeNowMs());
    }
    else
    {
      FaultManager_Set(FAULT_UI_STATE_ERROR);
    }
  }
  if (MenuController_TakeExitRequest())
  {
    DisplayController_SetPage(DISPLAY_PAGE_NET);
    (void)SystemContext_SetState(APP_STATE_RUN, BSP_TimeNowMs());
  }
}

static void App_100msTask(void *context)
{
  (void)context;
}

static void App_20msTask(void *context)
{
  (void)context;
  DeviceManager_Process20ms();
  App_PublishRawMeasurement();
  MetrologyManager_Process20ms();
  DisplayController_Process20ms();
  Stage3MetrologyDiagnostics_Update();
}

static void App_500msTask(void *context)
{
  (void)context;
  DeviceManager_Process500ms();
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
    case EVENT_CS1237_SAMPLE_AVAILABLE:
    case EVENT_TM1628_KEY_RAW_CHANGED:
    case EVENT_BATTERY_SAMPLE_UPDATED:
    case EVENT_W02_PWRKEY_PULSE_DONE:
    case EVENT_DRIVER_READY:
    case EVENT_DRIVER_ERROR:
    case EVENT_RAW_MEASUREMENT_UPDATED:
    case EVENT_CONFIG_SAVE_STARTED:
    case EVENT_CONFIG_SAVE_COMPLETED:
    case EVENT_CONFIG_SAVE_NO_CHANGE:
    case EVENT_CONFIG_SAVE_FAILED:
    case EVENT_CONFIG_LOAD_RECOVERED:
    case EVENT_CONFIG_LOAD_DEFAULTS:
    case EVENT_FACTORY_RESET_STARTED:
    case EVENT_FACTORY_RESET_COMPLETED:
    case EVENT_FACTORY_RESET_FAILED:
    case EVENT_NONE:
    default:
      break;
  }

  if (event->type == EVENT_TM1628_KEY_RAW_CHANGED)
  {
    Stage2B_DiagnosticsSetRawKeyMask((uint8_t)event->arg0);
  }
}

static void App_RunStateMachine(void)
{
  const SystemContext *context = SystemContext_Get();
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
        next_state = APP_STATE_LOAD_CONFIG;
        break;
      case APP_STATE_LOAD_CONFIG:
        next_state = APP_STATE_DEVICE_INIT;
        break;
      case APP_STATE_DEVICE_INIT:
        if (!s_device_manager_init_attempted)
        {
          s_device_manager_init_attempted = true;
          if ((context == NULL) || !DeviceManager_Init(&context->config))
          {
            FaultManager_Set(FAULT_CS1237_CONFIG_ERROR);
            next_state = APP_STATE_FAULT;
          }
          else
          {
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
            next_state = APP_STATE_WARMUP;
#else
            if (SelfTestController_Begin())
            {
              next_state = APP_STATE_SELF_TEST;
            }
            else
            {
              FaultManager_Set(FAULT_UI_DISPLAY_ERROR);
              next_state = APP_STATE_FAULT;
            }
#endif
          }
        }
        break;
      case APP_STATE_SELF_TEST:
        if (SelfTestController_GetState() == SELF_TEST_COMPLETE)
        {
          (void)DisplayController_Init();
          next_state = APP_STATE_WARMUP;
        }
        else if (SelfTestController_GetState() == SELF_TEST_FAILED)
        {
          FaultManager_Set(FAULT_UI_DISPLAY_ERROR);
          next_state = APP_STATE_FAULT;
        }
        break;
      case APP_STATE_WARMUP:
        if (DeviceManager_IsReady())
        {
#if (ENABLE_STAGE2B_BOARD_DIAGNOSTICS != 0U)
          next_state = APP_STATE_DIAGNOSTIC;
#else
          next_state = APP_STATE_RUN;
#endif
        }
        else if ((context != NULL) &&
                 BSP_TimeElapsed(BSP_TimeNowMs(), context->state_enter_time_ms,
                                 APP_DRIVER_WARMUP_TIMEOUT_MS))
        {
          FaultManager_Set(FAULT_CS1237_NOT_READY);
          next_state = APP_STATE_FAULT;
        }
        break;
      case APP_STATE_RUN:
      case APP_STATE_DIAGNOSTIC:
      case APP_STATE_MENU:
        break;
      case APP_STATE_CALIBRATION:
      {
        const CalibrationSession *session = CalibrationController_GetSession();
        if (((session->state == CAL_STATE_COMPLETE) ||
             (session->state == CAL_STATE_CANCELLED)) &&
            ((uint32_t)(BSP_TimeNowMs() - session->state_enter_ms) >=
             UI_MESSAGE_DEFAULT_MS))
        {
          next_state = APP_STATE_MENU;
          (void)MenuController_Enter();
        }
        break;
      }
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
    if (next_state == APP_STATE_DIAGNOSTIC)
    {
      Stage2B_DiagnosticsInit();
    }
  }

  if ((next_state == APP_STATE_FAULT) && !s_fault_entry_applied)
  {
    Stage2B_DiagnosticsEnterFault();
    SelfTestController_Cancel();
    MenuController_Cancel();
    CalibrationController_Cancel();
    DisplayController_SetPage(DISPLAY_PAGE_FAULT);
    DeviceManager_EnterSafeState();
    s_fault_entry_applied = true;
  }
}

static CommandResult App_ExecuteLocalCommand(CommandId id, int32_t value0)
{
  CommandRequest request = {id, COMMAND_SOURCE_LOCAL_KEY, value0, 0, 0U};
  CommandResponse response;
  return CommandService_Execute(&request, &response);
}

static void App_ShowCommandResult(CommandResult result, bool tare_action)
{
  DisplayCode code;
  char text[6];

  if ((result == COMMAND_RESULT_OK) || (result == COMMAND_RESULT_ACCEPTED))
  {
    code = DISPLAY_CODE_DONE;
  }
  else if (result == COMMAND_RESULT_NOT_CALIBRATED)
  {
    code = DISPLAY_CODE_NOCAL;
  }
  else if (result == COMMAND_RESULT_NOT_STABLE)
  {
    code = DISPLAY_CODE_UNSTABLE;
  }
  else if (result == COMMAND_RESULT_OVERLOAD)
  {
    code = DISPLAY_CODE_OVERLOAD;
  }
  else
  {
    code = tare_action ? DISPLAY_CODE_TARE_ERROR : DISPLAY_CODE_ZERO_ERROR;
  }
  if (DisplayCodes_Get(code, text))
  {
    DisplayController_ShowMessage(text, UI_MESSAGE_DEFAULT_MS);
  }
}

static void App_ProcessKeyEvent(const KeyEvent *event)
{
  AppState state = SystemContext_GetState();

  if ((event == NULL) || ((event->type != KEY_EVENT_SHORT) &&
      (event->type != KEY_EVENT_LONG) &&
      (event->type != KEY_EVENT_REPEAT)))
  {
    return;
  }
  if (state == APP_STATE_MENU)
  {
    (void)MenuController_HandleKeyEvent(event);
    return;
  }
  if (state == APP_STATE_CALIBRATION)
  {
    (void)CalibrationController_HandleKeyEvent(event);
    return;
  }
  if (state != APP_STATE_RUN)
  {
    return;
  }

  if ((event->key == KEY_ID_FUNCTION) &&
      (event->type == KEY_EVENT_SHORT))
  {
    DisplayPage page = DisplayController_GetPage();
    if (page == DISPLAY_PAGE_NET) page = DISPLAY_PAGE_GROSS;
    else if (page == DISPLAY_PAGE_GROSS) page = DISPLAY_PAGE_TARE;
    else if (page == DISPLAY_PAGE_TARE) page = DISPLAY_PAGE_BATTERY;
    else page = DISPLAY_PAGE_NET;
    DisplayController_SetPage(page);
    if (page == DISPLAY_PAGE_NET)
      (void)App_ExecuteLocalCommand(COMMAND_SET_WEIGHT_VIEW, WEIGHT_VIEW_NET);
    else if (page == DISPLAY_PAGE_GROSS)
      (void)App_ExecuteLocalCommand(COMMAND_SET_WEIGHT_VIEW, WEIGHT_VIEW_GROSS);
  }
  else if ((event->key == KEY_ID_FUNCTION) &&
           (event->type == KEY_EVENT_LONG))
  {
    if (MenuController_Enter())
      (void)SystemContext_SetState(APP_STATE_MENU, event->timestamp_ms);
  }
  else if ((event->key == KEY_ID_TARE) &&
           (event->type == KEY_EVENT_SHORT))
    App_ShowCommandResult(App_ExecuteLocalCommand(COMMAND_TARE, 0), true);
  else if ((event->key == KEY_ID_TARE) &&
           (event->type == KEY_EVENT_LONG))
    App_ShowCommandResult(App_ExecuteLocalCommand(COMMAND_CLEAR_TARE, 0), true);
  else if ((event->key == KEY_ID_ZERO) &&
           (event->type == KEY_EVENT_SHORT))
    App_ShowCommandResult(App_ExecuteLocalCommand(COMMAND_ZERO, 0), false);
  else if ((event->key == KEY_ID_ZERO) &&
           (event->type == KEY_EVENT_LONG))
    App_ShowCommandResult(App_ExecuteLocalCommand(COMMAND_RESET_ZERO, 0), false);
  else if ((event->key == KEY_ID_STAR) &&
           (event->type == KEY_EVENT_SHORT))
    App_ShowCommandResult(App_ExecuteLocalCommand(
        COMMAND_REQUEST_MANUAL_OUTPUT, 0), false);
  else if ((event->key == KEY_ID_STAR) &&
           (event->type == KEY_EVENT_LONG))
    DisplayController_SetPage(DISPLAY_PAGE_STATUS);
  else if ((event->key == KEY_ID_HASH) &&
           (event->type == KEY_EVENT_SHORT))
  {
    DisplayPage page = (DisplayController_GetPage() == DISPLAY_PAGE_GROSS) ?
                       DISPLAY_PAGE_NET : DISPLAY_PAGE_GROSS;
    DisplayController_SetPage(page);
    (void)App_ExecuteLocalCommand(COMMAND_SET_WEIGHT_VIEW,
        (page == DISPLAY_PAGE_NET) ? WEIGHT_VIEW_NET : WEIGHT_VIEW_GROSS);
  }
}

static void App_PublishRawMeasurement(void)
{
  AppEvent event;

  if (MeasurementBridge_BuildUpdateEvent(s_last_published_raw_count, &event) &&
      EventQueue_Push(&event))
  {
    s_last_published_raw_count = event.arg1;
  }
}

static bool App_PushStartupEvent(EventType type)
{
  return App_PushEvent(type, 0U, 0U);
}

static bool App_PushEvent(EventType type, uint32_t arg0, uint32_t arg1)
{
  AppEvent event;

  event.type = type;
  event.timestamp_ms = BSP_TimeNowMs();
  event.arg0 = arg0;
  event.arg1 = arg1;
  event.source = NULL;
  return EventQueue_Push(&event);
}
