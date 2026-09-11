#include "calibration_model.h"
#include "alarm_config_validation.h"
#include "default_config.h"
#include "device_config_validator.h"
#include "key_service.h"
#include "mass_math.h"
#include "metrology_config_validator.h"
#include "metrology_standard_validator.h"
#include "persistent_codec.h"
#include "persistent_schema.h"
#include "project_config.h"
#include "unit_converter.h"
#include "modbus_register_model.h"
#include "modbus_register_map.h"
#include "stage5a_model_adapters.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned failures;
#define CHECK(x) do { if (!(x)) { ++failures; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #x); } } while (0)

static void TestMassAndUnits(void)
{
    MassValueUg value;
    DisplayWeightValue display;
    UnitDisplayConfig kg={true,3U,1U}, g={true,0U,1U}, lb={true,3U,1U};
    CHECK(MassMath_Add(INT64_MAX,0,&value)&&value==INT64_MAX);
    CHECK(!MassMath_Add(INT64_MAX,1,&value));
    CHECK(MassMath_MulDivRound(INT64_MIN,1,1,&value)&&value==INT64_MIN);
    CHECK(UnitConverter_MassToDisplay(INT64_C(1000000000),MASS_UNIT_KG,&kg,&display)&&display.display_count==1000);
    CHECK(UnitConverter_MassToDisplay(INT64_C(1000000000),MASS_UNIT_G,&g,&display)&&display.display_count==1000);
    CHECK(UnitConverter_MassToDisplay(INT64_C(453592370),MASS_UNIT_LB,&lb,&display)&&display.display_count==1000);
    CHECK(UnitConverter_MassToDisplay(-INT64_C(453592370),MASS_UNIT_LB,&lb,&display)&&display.display_count==-1000);
    CHECK(UnitConverter_CountToMass(10000,MASS_UNIT_KG,3U,&value)&&value==INT64_C(10000000000));
    CHECK(UnitConverter_CountToMass(5000,MASS_UNIT_G,0U,&value)&&value==INT64_C(5000000000));
    CHECK(UnitConverter_CountToMass(1000,MASS_UNIT_LB,3U,&value)&&value==INT64_C(453592370));
    kg.division_digit=5U;
    CHECK(UnitConverter_MassToDisplay(INT64_C(1003000000),MASS_UNIT_KG,&kg,&display)&&display.display_count==1005);
}

static void TestAlarmExtremeValidation(void)
{
    DeviceConfig config;
    DeviceConfig original;
    RuntimeState runtime = {0};
    uint8_t bytes[PERSISTENT_V3_PAYLOAD_SIZE];
    uint16_t length = 0U;

    DefaultConfig_Load(&config);
    config.alarm.limit_function_enable = true;
    config.alarm.lower_limit_ug = INT64_MIN;
    config.alarm.upper_limit_ug = INT64_MAX;
    config.alarm.hysteresis_ug = 0;
    original = config;
    CHECK(AlarmConfig_Validate(&config.alarm));
    CHECK(DeviceConfig_Validate(&config));
    CHECK(memcmp(&config, &original, sizeof(config)) == 0);

    config.alarm.hysteresis_ug = INT64_MAX;
    CHECK(AlarmConfig_Validate(&config.alarm) ==
          DeviceConfig_Validate(&config));
    CHECK(DeviceConfig_Validate(&config));
    config.alarm.lower_limit_ug = INT64_MIN;
    config.alarm.upper_limit_ug = INT64_MIN;
    config.alarm.hysteresis_ug = 0;
    CHECK(!AlarmConfig_Validate(&config.alarm));
    CHECK(!DeviceConfig_Validate(&config));
    config.alarm.lower_limit_ug = INT64_MAX;
    config.alarm.upper_limit_ug = INT64_MIN;
    CHECK(!AlarmConfig_Validate(&config.alarm));
    CHECK(!DeviceConfig_Validate(&config));

    config.alarm.limit_function_enable = false;
    CHECK(AlarmConfig_Validate(&config.alarm));
    CHECK(DeviceConfig_Validate(&config));
    CHECK(PersistentCodec_EncodeV3(&config, &runtime, bytes, sizeof(bytes),
        &length) == PERSISTENT_CODEC_OK);

    config = original;
    CHECK(AlarmConfig_Validate(&config.alarm));
    CHECK(DeviceConfig_Validate(&config));
    CHECK(memcmp(&config, &original, sizeof(config)) == 0);
    CHECK(PersistentCodec_EncodeV3(&config, &runtime, bytes,
        sizeof(bytes), &length) == PERSISTENT_CODEC_OK);

    config.alarm.hysteresis_ug = INT64_MAX;
    CHECK(AlarmConfig_Validate(&config.alarm) == DeviceConfig_Validate(&config));
    CHECK(DeviceConfig_Validate(&config));
    CHECK(PersistentCodec_EncodeV3(&config, &runtime, bytes,
        sizeof(bytes), &length) == PERSISTENT_CODEC_OK);

    config.alarm.lower_limit_ug = INT64_MAX;
    config.alarm.upper_limit_ug = INT64_MAX;
    config.alarm.hysteresis_ug = 0;
    CHECK(!AlarmConfig_Validate(&config.alarm));
    CHECK(!DeviceConfig_Validate(&config));
    config.alarm.lower_limit_ug = 1;
    config.alarm.upper_limit_ug = 0;
    CHECK(!AlarmConfig_Validate(&config.alarm));
    CHECK(!DeviceConfig_Validate(&config));

    config.alarm.limit_function_enable = false;
    config.alarm.lower_limit_ug = INT64_MAX;
    config.alarm.upper_limit_ug = INT64_MIN;
    CHECK(AlarmConfig_Validate(&config.alarm));
    CHECK(DeviceConfig_Validate(&config));
    CHECK(PersistentCodec_EncodeV3(&config, &runtime, bytes,
        sizeof(bytes), &length) == PERSISTENT_CODEC_OK);
}

static void TestCodec(void)
{
    DeviceConfig config,decoded;
    RuntimeState runtime={0},decoded_runtime;
    uint8_t bytes[PERSISTENT_V3_PAYLOAD_SIZE];
    uint8_t roundtrip[PERSISTENT_V3_PAYLOAD_SIZE];
    uint16_t length=0U;
    uint16_t roundtrip_length=0U;
    PersistentCodecResult result;
    DefaultConfig_Load(&config);
    config.system.startup_auto_zero_enable=true;
    runtime.weight_view=WEIGHT_VIEW_NET;
    result=PersistentCodec_EncodeV3(&config,&runtime,bytes,sizeof(bytes),&length);
    CHECK(result==PERSISTENT_CODEC_OK);
    CHECK(length==PERSISTENT_V3_PAYLOAD_SIZE);
    if(result==PERSISTENT_CODEC_OK)
    {
        CHECK(PersistentCodec_DecodeV3(bytes,length,&decoded,&decoded_runtime)==PERSISTENT_CODEC_OK);
        CHECK(PersistentCodec_EncodeV3(&decoded,&decoded_runtime,roundtrip,
            sizeof(roundtrip),&roundtrip_length)==PERSISTENT_CODEC_OK);
        CHECK(roundtrip_length==length&&memcmp(bytes,roundtrip,length)==0);
        CHECK(!decoded_runtime.config_dirty);
        CHECK(decoded.system.startup_auto_zero_enable);
    }
    config.metrology.capacity_ug=INT64_C(10000000000);
    config.metrology.overload_threshold_ug=INT64_C(10000000000);
    config.metrology.load_cell.rated_capacity_known=false;
    config.metrology.active_unit=MASS_UNIT_KG;
    config.metrology.unit_display[MASS_UNIT_G].decimal_places=0U;
    CHECK(CalibrationModel_BuildMass(0,100000,INT64_C(10000000000),1U,
        &config.calibration)==CALIBRATION_RESULT_OK);
    config.system.tare_power_loss_retention=true;
    runtime.tare_active=true;
    runtime.current_tare=INT32_MAX;
    runtime.current_tare_ug=INT64_C(5000000000);
    CHECK(PersistentCodec_EncodeV3(&config,&runtime,bytes,sizeof(bytes),&length)==PERSISTENT_CODEC_OK);
    CHECK(PersistentCodec_DecodeV3(bytes,length,&decoded,&decoded_runtime)==PERSISTENT_CODEC_OK);
    CHECK(decoded_runtime.tare_active&&decoded_runtime.current_tare_ug==INT64_C(5000000000));
}

static void TestReferenceRules(void)
{
    DeviceConfig config;
    DefaultConfig_Load(&config);
    config.metrology.capacity_ug=INT64_C(10000000000);
    config.metrology.overload_threshold_ug=INT64_C(10000000000);
    config.metrology.load_cell.rated_capacity_known=false;
    config.metrology.active_unit=MASS_UNIT_KG;
    config.metrology.compliance_mode=METROLOGY_COMPLIANCE_CLASS_III_REFERENCE;
    CHECK(MetrologyStandardValidator_Validate(&config.metrology)==METROLOGY_STANDARD_OK);
    CHECK(MetrologyStandardValidator_GetMinimumLoad(&config.metrology)==INT64_C(20000000));
    CHECK(MetrologyStandardValidator_GetDisplayOverload(&config.metrology)==INT64_C(10009000000));
    config.metrology.active_unit=MASS_UNIT_LB;
    CHECK(MetrologyStandardValidator_Validate(&config.metrology)==METROLOGY_STANDARD_INVALID_UNIT);
}

static void TestProductDefaults(void)
{
    DeviceConfig config;

    DefaultConfig_Load(&config);
    CHECK(config.metrology.profiles[WEIGHING_PROFILE_HIGH_PRECISION].filter_mode ==
        FILTER_MODE_MEDIAN3_IIR);
    CHECK(config.metrology.profiles[WEIGHING_PROFILE_HIGH_PRECISION].filter_strength == 3U);
    CHECK(config.metrology.profiles[WEIGHING_PROFILE_HIGH_PRECISION].sample_rate ==
        DEVICE_CS1237_DATA_RATE_10_HZ);
    CHECK(config.metrology.capacity_ug==INT64_C(3000000000));
    CHECK(config.metrology.overload_threshold_ug==INT64_C(3000000000));
    CHECK(config.metrology.zero_range_ug==INT64_C(60000000));
    CHECK(config.metrology.load_cell.rated_capacity_known);
    CHECK(config.metrology.load_cell.rated_capacity_ug==INT64_C(3000000000));
    CHECK(config.metrology.active_unit==MASS_UNIT_G);
    CHECK(config.metrology.unit_display[MASS_UNIT_G].decimal_places==2U);
    CHECK(config.metrology.profiles[WEIGHING_PROFILE_HIGH_PRECISION]
        .stability_enter_threshold_ug==INT64_C(50000));
    CHECK(config.metrology.profiles[WEIGHING_PROFILE_HIGH_PRECISION]
        .stability_exit_threshold_ug==INT64_C(100000));
    CHECK(config.metrology.profiles[WEIGHING_PROFILE_HIGH_PRECISION]
        .stability_hold_ms==1000U);
    CHECK(MetrologyConfig_ValidateProductHardware(&config.metrology)==
        METROLOGY_CONFIG_OK);
    config.metrology.profiles[WEIGHING_PROFILE_HIGH_SPEED].sample_rate =
        DEVICE_CS1237_DATA_RATE_640_HZ;
    CHECK(MetrologyConfig_ValidateCanonical(&config.metrology) ==
        METROLOGY_CONFIG_OK);
    CHECK(MetrologyConfig_ValidateProductHardware(&config.metrology) ==
        METROLOGY_CONFIG_INVALID_PROFILE);
    config.metrology.profiles[WEIGHING_PROFILE_HIGH_SPEED].sample_rate =
        DEVICE_CS1237_DATA_RATE_1280_HZ;
    CHECK(MetrologyConfig_ValidateProductHardware(&config.metrology) ==
        METROLOGY_CONFIG_INVALID_PROFILE);
    config.metrology.profiles[WEIGHING_PROFILE_HIGH_SPEED].sample_rate =
        DEVICE_CS1237_DATA_RATE_40_HZ;
    config.metrology.overload_threshold_ug=INT64_C(3000000001);
    CHECK(MetrologyConfig_ValidateProductHardware(&config.metrology)==
        METROLOGY_CONFIG_INVALID_OVERLOAD);
    config.metrology.overload_threshold_ug=INT64_C(2999999999);
    CHECK(MetrologyConfig_ValidateProductHardware(&config.metrology)==
        METROLOGY_CONFIG_INVALID_OVERLOAD);
}

static void TestKeyConflict(void)
{
    KeyMap map={{0U,1U,2U,3U,4U}};
    KeyEvent event;
    CHECK(KeyService_Init(&map));
    KeyService_Process10ms(0x03U,0U);
    CHECK(KeyService_IsConflictActive());
    CHECK(KeyService_GetMultiKeyConflictCount()==1U);
    CHECK(!KeyService_TryPopEvent(&event));
    KeyService_Process10ms(0U,10U);
    KeyService_Process10ms(0U,40U);
    CHECK(!KeyService_IsConflictActive());
}

static void TestModbusModel(void)
{
    uint16_t words[4];
    uint16_t drift_words[30];
    uint16_t request[]={1U,1U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0xA55AU};
    Stage5A_ModelAdaptersInit();
    CHECK(ModbusRegisterModel_ReadHolding(0x013EU, 1U, words) ==
          MODBUS_REGISTER_OK && words[0] == DEVICE_CONFIG_SCHEMA_VERSION);
    CHECK(ModbusRegisterModel_ReadHolding(0x01C0U, 1U, words) ==
          MODBUS_REGISTER_OK && words[0] == CONFIG_STORE_SCHEMA_V3);
    CHECK(ModbusRegisterModel_ReadHolding(0x000FU, 1U, words) ==
          MODBUS_REGISTER_OK && words[0] == FW_RELEASE_VERSION);
    Stage5A_ModelSnapshot()->net_mass_ug=INT64_C(0x1122334455667788);
    Stage5A_ModelDisplayCondition()->state=DISPLAY_CONDITION_LOCKED;
    Stage5A_ModelDisplayCondition()->locked=true;
    Stage5A_ModelDisplayCondition()->display_mass_ug=INT64_C(1000000000);
    Stage5A_ModelDisplayCondition()->anchor_mass_ug=INT64_C(0x0102030405060708);
    Stage5A_ModelDisplayCondition()->release_threshold_ug=INT64_C(8000000);
    Stage5A_ModelDisplayCondition()->candidate_elapsed_ms=0x12345678U;
    Stage5A_ModelDisplayCondition()->last_release_reason=DISPLAY_RELEASE_DEVIATION;
    Stage5A_ModelSnapshot()->uncompensated_gross_mass_ug=INT64_C(0x1020304050607080);
    Stage5A_ModelRuntimeDrift()->state=RUNTIME_DRIFT_TRACKING;
    Stage5A_ModelRuntimeDrift()->enabled=true;
    Stage5A_ModelRuntimeDrift()->offset_ug=INT64_C(0x0102030405060708);
    Stage5A_ModelRuntimeDrift()->arming_elapsed_ms=0xA1B2C3D4U;
    Stage5A_ModelRuntimeDrift()->stable_sample_count=0x10203040U;
    Stage5A_ModelContext()->config.system.startup_auto_zero_enable=true;
    Stage5A_ModelStartupAutoZero()->state=STARTUP_AUTO_ZERO_APPLIED;
    Stage5A_ModelStartupAutoZero()->enabled_at_boot=true;
    Stage5A_ModelStartupAutoZero()->terminal=true;
    Stage5A_ModelStartupAutoZero()->last_zero_result=WEIGHT_ACTION_OK;
    Stage5A_ModelStartupAutoZero()->elapsed_ms=0x11223344U;
    Stage5A_ModelStartupAutoZero()->observed_gross_mass_ug=
        INT64_C(0x0102030405060708);
    ModbusRegisterModel_Init();
    CHECK(ModbusRegisterModel_ReadHolding(0x0000U,2U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==1U&&words[1]==0x86A0U);
    CHECK(ModbusRegisterModel_ReadHolding(0x000EU,1U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==MODBUS_REGISTER_MAP_VERSION);
    CHECK(ModbusRegisterModel_ReadHolding(0x013CU,1U,words)==MODBUS_REGISTER_OK&&
        words[0]==1U);
    CHECK(ModbusRegisterModel_ReadHolding(0x017CU,1U,words)==MODBUS_REGISTER_OK&&
        words[0]==1U);
    CHECK(ModbusRegisterModel_WriteSingle(0x017CU,2U,COMMAND_SOURCE_MODBUS)==
        MODBUS_REGISTER_ILLEGAL_VALUE);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_RUNTIME_DRIFT_STATE,2U,
        words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==RUNTIME_DRIFT_TRACKING&&words[1]==1U);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_RUNTIME_DRIFT_RESERVED,1U,
        words)==MODBUS_REGISTER_OK&&words[0]==0U);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_RUNTIME_DRIFT_FIRST,6U,
        drift_words)==MODBUS_REGISTER_OK);
    CHECK(drift_words[0]==RUNTIME_DRIFT_TRACKING&&drift_words[1]==1U&&
        drift_words[2]==0U&&drift_words[3]==0U&&
        drift_words[4]==0x0102U&&drift_words[5]==0x0304U);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_RUNTIME_DRIFT_LIMITED,3U,
        drift_words)==MODBUS_REGISTER_OK);
    CHECK(drift_words[0]==0U&&drift_words[1]==0U&&drift_words[2]==0x0102U);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_RUNTIME_DRIFT_RESERVED,2U,
        drift_words)==MODBUS_REGISTER_OK);
    CHECK(drift_words[0]==0U&&drift_words[1]==0x0102U);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_RUNTIME_DRIFT_OFFSET_FIRST,
        4U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==0x0102U&&words[1]==0x0304U&&
        words[2]==0x0506U&&words[3]==0x0708U);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_RUNTIME_DRIFT_FIRST,30U,
        drift_words)==MODBUS_REGISTER_OK);
    CHECK(drift_words[3]==0U&&drift_words[28]==0x1020U&&
        drift_words[29]==0x3040U);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_RUNTIME_DRIFT_LAST,1U,
        words)==MODBUS_REGISTER_OK&&words[0]==0x3040U);
    CHECK(ModbusRegisterModel_ReadHolding((uint16_t)(MODBUS_RUNTIME_DRIFT_LAST+1U),
        1U,words)==MODBUS_REGISTER_ILLEGAL_ADDRESS);
    CHECK(ModbusRegisterModel_WriteSingle(MODBUS_RUNTIME_DRIFT_RESERVED,
        0U,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_READ_ONLY);
    CHECK(ModbusRegisterModel_WriteSingle(MODBUS_RUNTIME_DRIFT_OFFSET_FIRST,
        0U,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_READ_ONLY);
    CHECK(ModbusRegisterModel_ReadHolding(0x0010U,4U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==0x1122U&&words[1]==0x3344U&&words[2]==0x5566U&&words[3]==0x7788U);
    Stage5A_ModelContext()->config.communication.word_order=MODBUS_WORD_ORDER_LOW_WORD_FIRST;
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_RUNTIME_DRIFT_OFFSET_FIRST,
        4U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==0x0708U&&words[1]==0x0506U&&
        words[2]==0x0304U&&words[3]==0x0102U);
    CHECK(ModbusRegisterModel_ReadHolding(0x0010U,4U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==0x7788U&&words[1]==0x5566U&&words[2]==0x3344U&&words[3]==0x1122U);
    Stage5A_ModelDisplayCondition()->operator_zero_anchor=true;
    Stage5A_ModelDisplayCondition()->display_mass_ug=0;
    CHECK(ModbusRegisterModel_ReadHolding(0x0000U,2U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==0U&&words[1]==0U);
    CHECK(ModbusRegisterModel_ReadHolding(0x0010U,4U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==0x7788U&&words[1]==0x5566U&&words[2]==0x3344U&&words[3]==0x1122U);
    Stage5A_ModelDisplayCondition()->display_mass_ug=INT64_C(1000000000);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_DISPLAY_CONDITION_STATE,2U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==DISPLAY_CONDITION_LOCKED&&words[1]==1U);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_DISPLAY_CONDITION_ANCHOR_FIRST,4U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==0x0708U&&words[1]==0x0506U&&words[2]==0x0304U&&words[3]==0x0102U);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_DISPLAY_CONDITION_ELAPSED_FIRST,2U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==0x5678U&&words[1]==0x1234U);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_DISPLAY_CONDITION_RELEASE_REASON,1U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==DISPLAY_RELEASE_DEVIATION);
    CHECK(ModbusRegisterModel_WriteSingle(MODBUS_DISPLAY_CONDITION_STATE,0U,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_READ_ONLY);
    CHECK(ModbusRegisterModel_WriteMultiple(0x0040U,12U,request,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_OK);
    CHECK(Stage5A_ModelCommandCount()==1U);
    CHECK(ModbusRegisterModel_WriteSingle(0x004BU,0xA55AU,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_OK);
    CHECK(Stage5A_ModelCommandCount()==1U);
    request[0]=2U; request[1]=25U; request[2]=0U; request[3]=1U;
    CHECK(ModbusRegisterModel_WriteMultiple(0x0040U,12U,request,COMMAND_SOURCE_MODBUS)==
        MODBUS_REGISTER_OK);
    CHECK(Stage5A_ModelCommandCount()==2U);
    CHECK(Stage5A_ModelLastCommand()->id==COMMAND_SET_RUNTIME_DRIFT_ENABLED&&
        Stage5A_ModelLastCommand()->value0==1);
    request[0]=3U; request[1]=26U; request[2]=0U; request[3]=0U;
    CHECK(ModbusRegisterModel_WriteMultiple(0x0040U,12U,request,COMMAND_SOURCE_MODBUS)==
        MODBUS_REGISTER_OK);
    CHECK(Stage5A_ModelLastCommand()->id==COMMAND_RUNTIME_DRIFT_ENABLE);
    request[0]=4U; request[1]=27U;
    CHECK(ModbusRegisterModel_WriteMultiple(0x0040U,12U,request,COMMAND_SOURCE_MODBUS)==
        MODBUS_REGISTER_OK);
    CHECK(Stage5A_ModelLastCommand()->id==COMMAND_RUNTIME_DRIFT_DISABLE);
    request[0]=5U; request[1]=28U;
    CHECK(ModbusRegisterModel_WriteMultiple(0x0040U,12U,request,COMMAND_SOURCE_MODBUS)==
        MODBUS_REGISTER_OK);
    CHECK(Stage5A_ModelLastCommand()->id==COMMAND_RUNTIME_DRIFT_RESET);
    CHECK(ModbusRegisterModel_WriteSingle(0x004BU,0xA55AU,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_OK);
    CHECK(Stage5A_ModelCommandCount()==5U);
    CHECK(ModbusRegisterModel_ReadHolding(MODBUS_STARTUP_ZERO_FIRST,10U,
        drift_words)==MODBUS_REGISTER_OK);
    CHECK(drift_words[0]==STARTUP_AUTO_ZERO_APPLIED&&drift_words[1]==1U&&
        drift_words[2]==1U&&drift_words[3]==WEIGHT_ACTION_OK&&
        drift_words[4]==0x3344U&&drift_words[5]==0x1122U&&
        drift_words[6]==0x0708U&&drift_words[9]==0x0102U);
    CHECK(ModbusRegisterModel_WriteSingle(MODBUS_STARTUP_ZERO_STATE,0U,
        COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_READ_ONLY);
    CHECK(ModbusRegisterModel_WriteSingle(0x0100U,1U,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_READ_ONLY);
    CHECK(ModbusRegisterModel_WriteMultiple(0x0140U,2U,words,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_ReadHolding(0x017FU,1U,words)==MODBUS_REGISTER_OK&&words[0]==1U);
    words[0]=2U; words[1]=5U;
    CHECK(ModbusRegisterModel_WriteMultiple(0x01A1U,2U,words,COMMAND_SOURCE_MODBUS)==MODBUS_REGISTER_ILLEGAL_VALUE);
    CHECK(ModbusRegisterModel_ReadHolding(0x01A1U,1U,words)==MODBUS_REGISTER_OK&&words[0]==1U);
}

static CommandResult ExecuteMailbox(uint16_t token, uint16_t command)
{
    uint16_t request[12] = {0};
    uint16_t result = COMMAND_RESULT_INTERNAL_ERROR;
    request[0] = token;
    request[1] = command;
    request[11] = MODBUS_EXECUTE_VALUE;
    CHECK(ModbusRegisterModel_WriteMultiple(0x0040U, 12U, request,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_ReadHolding(0x004DU, 1U, &result) ==
        MODBUS_REGISTER_OK);
    return (CommandResult)result;
}

static void TestModbusAlarmExtremePath(void)
{
    static const uint16_t minimum[4] = {0x8000U, 0U, 0U, 0U};
    static const uint16_t maximum[4] = {0x7FFFU, 0xFFFFU, 0xFFFFU, 0xFFFFU};
    DeviceConfig active_before_invalid;
    RuntimeState runtime = {0};
    uint8_t payload[PERSISTENT_V3_PAYLOAD_SIZE];
    uint16_t payload_length = 0U;
    uint32_t revision;
    bool dirty;

    Stage5A_ModelAdaptersInit();
    ModbusRegisterModel_Init();
    CHECK(ModbusRegisterModel_WriteSingle(0x0240U, 1U,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_WriteMultiple(0x0242U, 4U, minimum,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_WriteMultiple(0x0246U, 4U, maximum,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_WriteMultiple(0x024AU, 4U, maximum,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ExecuteMailbox(100U, 10U) == COMMAND_RESULT_OK);
    CHECK(ExecuteMailbox(101U, 11U) == COMMAND_RESULT_OK);
    CHECK(Stage5A_ModelContext()->config.alarm.lower_limit_ug == INT64_MIN);
    CHECK(Stage5A_ModelContext()->config.alarm.upper_limit_ug == INT64_MAX);
    CHECK(Stage5A_ModelContext()->config.alarm.hysteresis_ug == INT64_MAX);
    runtime.weight_view = WEIGHT_VIEW_NET;
    CHECK(PersistentCodec_EncodeV3(&Stage5A_ModelContext()->config, &runtime,
        payload, sizeof(payload), &payload_length) == PERSISTENT_CODEC_OK);

    active_before_invalid = Stage5A_ModelContext()->config;
    revision = Stage5A_ModelContext()->config_revision;
    dirty = Stage5A_ModelContext()->runtime.config_dirty;
    CHECK(ExecuteMailbox(102U, 9U) == COMMAND_RESULT_OK);
    CHECK(ModbusRegisterModel_WriteSingle(0x0240U, 1U,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_WriteMultiple(0x0242U, 4U, minimum,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_WriteMultiple(0x0246U, 4U, minimum,
        COMMAND_SOURCE_MODBUS) == MODBUS_REGISTER_OK);
    CHECK(ExecuteMailbox(103U, 10U) == COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK(ExecuteMailbox(104U, 11U) == COMMAND_RESULT_INVALID_ARGUMENT);
    CHECK(memcmp(&Stage5A_ModelContext()->config, &active_before_invalid,
                 sizeof(active_before_invalid)) == 0);
    CHECK(Stage5A_ModelContext()->config_revision == revision);
    CHECK(Stage5A_ModelContext()->runtime.config_dirty == dirty);
}

int main(void)
{
    TestMassAndUnits(); TestAlarmExtremeValidation(); TestCodec();
    TestReferenceRules(); TestProductDefaults();
    TestKeyConflict(); TestModbusModel(); TestModbusAlarmExtremePath();
    if(failures==0U) printf("Stage 5A host tests passed.\n");
    return failures==0U?0:1;
}
