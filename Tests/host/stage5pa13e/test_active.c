#include "app_main.h"
#include "calibration_model.h"
#include "command_service.h"
#include "config_application.h"
#include "default_config.h"
#include "fault_manager.h"
#include "metrology_manager.h"
#include "menu_controller.h"
#include "mock_hal.h"
#include "r5_local_control.h"
#include "system_context.h"
#include <stdio.h>
#include <stdlib.h>

#define CHECK(x) do { if (!(x)) { (void)fprintf(stderr,"line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static GuardedCheckweigh alarm;
static uint32_t tick;
static uint32_t alarm_calls;
bool App_GetGuardedCheckweighState(GuardedCheckweigh *s) { *s=alarm; return true; }
bool App_A13CheckweighIsOff(void) { return alarm.mode == GUARDED_CHECKWEIGH_OFF; }
bool App_SetGuardedCheckweighMode(GuardedCheckweighMode mode,uint32_t expected,bool require)
{ ++alarm_calls; return GuardedCheckweigh_SetMode(&alarm,mode,expected,require); }
bool App_RestoreGuardedCheckweighMode(GuardedCheckweighMode mode)
{ return GuardedCheckweigh_SetMode(&alarm,mode,0U,false); }

static void init(void)
{
    DeviceConfig config;
    DefaultConfig_Load(&config);
    config.metrology.capacity_ug=INT64_C(3000000000);
    config.metrology.load_cell.rated_capacity_known=true;
    config.metrology.load_cell.rated_capacity_ug=INT64_C(3000000000);
    config.metrology.profiles[0].sample_rate=DEVICE_CS1237_DATA_RATE_10_HZ;
    config.metrology.profiles[0].filter_mode=FILTER_MODE_AVERAGE;
    config.metrology.profiles[0].filter_strength=3U;
    CHECK(CalibrationModel_BuildMass(100000,600000,INT64_C(500000000),1U,&config.calibration)==CALIBRATION_RESULT_OK);
    config.system.requested_r5_application=1U;
    config.system.requested_r5_mode=2U;
    TestMock_Reset(); FaultManager_Init(); GuardedCheckweigh_Init(&alarm);
    CHECK(SystemContext_Init(&config,0U));
    CHECK(MetrologyManager_Init(&config,&SystemContext_Get()->runtime));
    CHECK(SystemContext_SetState(APP_STATE_RUN,0U));
    CommandService_Init();
    CHECK(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION));
    tick=0U; alarm_calls=0U;
}
static void sample(int32_t raw)
{
    RawMeasurementSample s={raw,(++tick)*100U,true};
    const WeightSnapshot *w;
    int64_t expected_display_input;
    CHECK(MetrologyManager_AcceptRawSample(&s));
    w=MetrologyManager_GetSnapshot();
    CHECK(MetrologyManager_GetSnapshot()->net_mass_ug ==
        MetrologyManager_GetSnapshot()->gross_mass_ug-MetrologyManager_GetSnapshot()->tare_mass_ug);
    CHECK(MetrologyManager_GetSnapshot()->gross_mass_ug ==
        MetrologyManager_GetSnapshot()->uncompensated_gross_mass_ug-
        (MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_ACTIVE ? MetrologyManager_GetA13CSnapshot()->offset_ug : 0));
    expected_display_input=SystemContext_Get()->runtime.weight_view==WEIGHT_VIEW_GROSS ?
        w->gross_mass_ug : w->net_mass_ug;
    CHECK(MetrologyManager_GetDisplayConditionSnapshot()->authoritative_mass_ug==expected_display_input);
}
static void ready(void) { uint32_t i; for(i=0U;i<30U;++i) sample(100000); }
static CommandResult cmd(CommandId id,int32_t app,int32_t mode,uint32_t gen)
{
    CommandRequest q={0}; CommandResponse r;
    q.source=COMMAND_SOURCE_MODBUS; q.id=id; q.value0=app;q.value1=mode;
    if(id==COMMAND_A13_SET_PAIR) {q.flags=1U;q.value64=gen;}
    return CommandService_Execute(&q,&r);
}
static void drift(void)
{
    uint32_t i; for(i=0U;i<4000U;++i) sample(100000+(int32_t)(i/25U));
    CHECK(MetrologyManager_GetA13CSnapshot()->offset_ug>0);
}
static void test_boot_shadow_and_nonzero_rejection(void)
{
    uint32_t revision,generation; int64_t offset;
    init(); ready(); revision=SystemContext_GetConfigRevision();
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    CHECK(MetrologyManager_GetA13CSnapshot()->mode==A13C_MODE_OFF);
    CHECK(!SystemContext_Get()->runtime.config_dirty);
    CHECK(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_SHADOW,R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
    drift(); offset=MetrologyManager_GetA13CSnapshot()->offset_ug;
    generation=MetrologyManager_GetA13Generation();
    CHECK(!MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION,generation,true));
    CHECK(MetrologyManager_GetA13CSnapshot()->offset_ug==offset);
    CHECK(MetrologyManager_GetA13Generation()==generation);
    CHECK(MetrologyManager_GetSnapshot()->gross_mass_ug==MetrologyManager_GetSnapshot()->uncompensated_gross_mass_ug);
    CHECK(SystemContext_GetConfigRevision()==revision);
    CHECK(!SystemContext_Get()->runtime.config_dirty);
    CHECK(TestMock_GetSaveRequestCount()==0U);
    CHECK(MetrologyManager_SetR5Mode(R5_DRIFT_MODE_OFF));
    CHECK(MetrologyManager_GetA13CSnapshot()->offset_ug==0);
}
static void test_output_dosing_exit_and_tare(void)
{
    int64_t offset,before; uint32_t i;
    init(); ready();
    CHECK(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
    drift(); offset=MetrologyManager_GetA13CSnapshot()->offset_ug;
    CHECK(MetrologyManager_SetR5Mode(R5_DRIFT_MODE_DOSING_NO_COMPENSATION));
    for(i=0U;i<100U;++i) { sample(i<50U?600159:100159);CHECK(MetrologyManager_GetA13CSnapshot()->offset_ug==offset); }
    for(i=0U;i<50U;++i) sample(600159);
    CHECK(MetrologyManager_Tare()==WEIGHT_ACTION_OK);
    CHECK(MetrologyManager_GetSnapshot()->net_mass_ug==0);
    CHECK(MetrologyManager_GetA13CSnapshot()->offset_ug==offset);
    CHECK(MetrologyManager_ClearTare()==WEIGHT_ACTION_OK);
    CHECK(MetrologyManager_GetA13CSnapshot()->offset_ug==offset);
    CHECK(MetrologyManager_SetR5Mode(R5_DRIFT_MODE_STATIC_COMPENSATION));
    sample(600159); CHECK(MetrologyManager_GetA13CSnapshot()->offset_ug==offset);
    before=MetrologyManager_GetSnapshot()->gross_mass_ug;
    CHECK(MetrologyManager_SetR5Mode(R5_DRIFT_MODE_OFF));
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    CHECK(MetrologyManager_GetSnapshot()->gross_mass_ug-before==offset);
    CHECK((int32_t)MetrologyManager_ReadA13ActiveMetric(2U)==offset);
    CHECK(MetrologyManager_GetA13CSnapshot()->reference_twice_ug==0);
    CHECK(MetrologyManager_GetDisplayConditionSnapshot()->display_mass_ug==MetrologyManager_GetSnapshot()->net_mass_ug);
}
static void test_conflicts_ble_and_checkweigh(void)
{
    uint32_t gen;CommandRequest q={0};CommandResponse r;
    init();ready();gen=MetrologyManager_GetA13Generation();
    CHECK(cmd(COMMAND_A13_SET_PAIR,1,2,gen)==COMMAND_RESULT_OK);
    CHECK(cmd(COMMAND_A13_SET_PAIR,0,0,gen)==COMMAND_RESULT_BUSY);
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_ACTIVE);
    CHECK(cmd(COMMAND_CHECKWEIGH_SET_MODE,1,0,0)==COMMAND_RESULT_INVALID_STATE);
    CHECK(cmd(COMMAND_CHECKWEIGH_SET_MODE,2,0,0)==COMMAND_RESULT_INVALID_STATE);
    CHECK(alarm_calls==0U);
    q.id=COMMAND_A13_SET_PAIR;q.source=COMMAND_SOURCE_BLE;q.value0=1;q.value1=2;
    CHECK(CommandService_Execute(&q,&r)==COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK(MetrologyManager_SetR5Mode(R5_DRIFT_MODE_OFF));
    CHECK(cmd(COMMAND_CHECKWEIGH_SET_MODE,1,0,0)==COMMAND_RESULT_OK);
    CHECK(!MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    CHECK(cmd(COMMAND_CHECKWEIGH_SET_MODE,0,0,0)==COMMAND_RESULT_OK);
    CHECK(R5LocalControl_BeginSession()); CHECK(R5LocalControl_Begin(false));
    R5LocalControl_Adjust(true);R5LocalControl_Adjust(true);R5LocalControl_Confirm();
    CHECK(MetrologyManager_SetR5Mode(R5_DRIFT_MODE_STATIC_COMPENSATION));
    CHECK(MetrologyManager_SetR5Mode(R5_DRIFT_MODE_OFF)); /* ABA */
    CHECK(R5LocalControl_Apply()==R5_LOCAL_RESULT_BUSY);
    CHECK(TestMock_GetSaveRequestCount()==0U);
}
static void test_failure_shutdown_and_manual_rearm(void)
{
    RawMeasurementSample bad={100000,0U,false};uint32_t i;
    init();ready();CHECK(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
    bad.timestamp_ms=(++tick)*100U;CHECK(!MetrologyManager_AcceptRawSample(&bad));
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    CHECK(MetrologyManager_GetA13CSnapshot()->mode==A13C_MODE_OFF);
    for(i=0U;i<50U;++i) sample(100000);
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    CHECK(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
    sample(100000); tick+=3U;sample(100000);
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    CHECK(MetrologyManager_ReadA13ActiveMetric(3U)==A13C_REASON_TIME_GAP);
    CHECK(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
    FaultManager_Set(FAULT_WEIGHT_MATH_OVERFLOW);MetrologyManager_HandleFaultState();
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    CHECK(!MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
    FaultManager_Init(); sample(100000);
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    CHECK(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
    CHECK(MetrologyManager_ReconfigureFilter(FILTER_MODE_IIR,3U));
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    ready(); CHECK(!MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
}
static void key(KeyId id,KeyEventType type,uint32_t *now)
{
    KeyEvent event={id,type,(*now+=20U),0U};
    CHECK(MenuController_HandleKeyEvent(&event));
}
static void menu_drift(uint32_t *now)
{
    uint32_t i;
    CHECK(SystemContext_SetState(APP_STATE_MENU,*now));
    MenuController_Init();CHECK(MenuController_Enter());
    key(KEY_ID_STAR,KEY_EVENT_SHORT,now);key(KEY_ID_HASH,KEY_EVENT_SHORT,now);
    key(KEY_ID_STAR,KEY_EVENT_SHORT,now);key(KEY_ID_HASH,KEY_EVENT_SHORT,now);
    CHECK(MenuController_IsAdvanced());
    for(i=0U;i<40U&&MenuController_GetItem()!=MENU_ITEM_R5_DRIFT;++i) key(KEY_ID_HASH,KEY_EVENT_SHORT,now);
    CHECK(MenuController_GetItem()==MENU_ITEM_R5_DRIFT);
    key(KEY_ID_FUNCTION,KEY_EVENT_SHORT,now);
}
static void test_menu_direct_long_volatile_and_cancel(void)
{
    uint32_t now=0U,revision;
    init();ready();revision=SystemContext_GetConfigRevision();menu_drift(&now);
    key(KEY_ID_HASH,KEY_EVENT_SHORT,&now);key(KEY_ID_HASH,KEY_EVENT_SHORT,&now);
    key(KEY_ID_FUNCTION,KEY_EVENT_LONG,&now);
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_ACTIVE);
    CHECK(MetrologyManager_GetA13CSnapshot()->mode==A13C_MODE_STATIC);
    CHECK(TestMock_GetSaveRequestCount()==0U);
    CHECK(SystemContext_GetConfigRevision()==revision);
    CHECK(!SystemContext_Get()->runtime.config_dirty);
    CHECK(SystemContext_Get()->config.system.requested_r5_application==1U); /* unchanged old request */
    MenuController_Cancel();CHECK(MetrologyManager_SetR5Mode(R5_DRIFT_MODE_OFF));
    menu_drift(&now);key(KEY_ID_HASH,KEY_EVENT_SHORT,&now);key(KEY_ID_TARE,KEY_EVENT_SHORT,&now);
    CHECK(!R5LocalControl_HasCandidate());CHECK(TestMock_GetSaveRequestCount()==0U);
    MenuController_Cancel();
    menu_drift(&now);key(KEY_ID_HASH,KEY_EVENT_SHORT,&now);
    TestMock_SetTimeMs(now+MENU_TIMEOUT_MS+10U);
    MenuController_Process10ms();
    CHECK(!MenuController_IsActive());
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    CHECK(TestMock_GetSaveRequestCount()==0U);
    CHECK(SystemContext_GetConfigRevision()==revision);
}
static void test_zero_calibration_and_overload(void)
{
    const CalibrationConfig *cal;uint32_t i;
    init();ready();CHECK(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
    drift();CHECK(MetrologyManager_Zero()==WEIGHT_ACTION_OK);
    CHECK(MetrologyManager_GetA13CSnapshot()->offset_ug==0);
    CHECK(MetrologyManager_GetA13CSnapshot()->reference_twice_ug==0);
    cal=&SystemContext_Get()->config.calibration;
    CHECK(MetrologyManager_ApplyCalibration(cal));
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    init();ready();CHECK(MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,R5_DRIFT_MODE_STATIC_COMPENSATION,0U,false));
    for(i=0U;i<10U;++i) sample(3400000);
    CHECK(MetrologyManager_GetR5Application()==R5_BETA_APPLICATION_SHADOW);
    CHECK(MetrologyManager_GetA13CSnapshot()->offset_ug==0);
    CHECK(MetrologyManager_GetSnapshot()->gross_mass_ug==MetrologyManager_GetSnapshot()->uncompensated_gross_mass_ug);
}
int main(void)
{
    test_boot_shadow_and_nonzero_rejection(); test_output_dosing_exit_and_tare();
    test_conflicts_ble_and_checkweigh();test_failure_shutdown_and_manual_rearm();
    test_menu_direct_long_volatile_and_cancel();test_zero_calibration_and_overload();
    (void)puts("A13E real metrology/command/output integration PASS");return 0;
}
