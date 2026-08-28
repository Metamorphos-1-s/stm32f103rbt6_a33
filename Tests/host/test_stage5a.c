#include "calibration_model.h"
#include "default_config.h"
#include "key_service.h"
#include "mass_math.h"
#include "metrology_config_validator.h"
#include "metrology_legacy_projection.h"
#include "metrology_standard_validator.h"
#include "persistent_codec.h"
#include "persistent_schema.h"
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

static void TestCanonicalAndLegacyBoundary(void)
{
    DeviceConfig config;
    RuntimeState runtime={0};
    uint8_t bytes[PERSISTENT_V2_PAYLOAD_SIZE];
    uint16_t length=0U;
    DefaultConfig_Load(&config);
    CHECK(!config.system.startup_auto_zero_enable);
    config.system.startup_auto_zero_enable=true;
    config.metrology.capacity=0U;
    config.metrology.division=0U;
    config.metrology.decimal_places=0U;
    config.metrology.filter_mode=FILTER_MODE_COUNT;
    config.metrology.filter_strength=0xFFU;
    config.metrology.zero_range=0U;
    config.metrology.overload_threshold=0U;
    config.stability.enter_threshold=UINT32_MAX;
    config.stability.exit_threshold=0U;
    CHECK(MetrologyConfig_ValidateCanonical(&config.metrology)==METROLOGY_CONFIG_OK);
    CHECK(PersistentCodec_ValidateConfig(&config));
    runtime.weight_view=WEIGHT_VIEW_NET;
    CHECK(PersistentCodec_EncodeV2(&config,&runtime,bytes,sizeof(bytes),&length)==PERSISTENT_CODEC_OK);
    CHECK(length==PERSISTENT_V2_PAYLOAD_SIZE);
    CHECK(MetrologyLegacyV1_Validate(&config.metrology,&config.stability)!=METROLOGY_CONFIG_OK);
    CHECK(MetrologyLegacyProjection_Update(&config.metrology));
    CHECK(MetrologyLegacyStabilityProjection_Update(&config.metrology,&config.stability));
    CHECK(MetrologyLegacyV1_Validate(&config.metrology,&config.stability)==METROLOGY_CONFIG_OK);
}

static void TestUnboundedLegacyProjection(void)
{
    DeviceConfig config;
    DeviceConfig decoded;
    MetrologyConfig projection_only;
    RuntimeState runtime = {0};
    RuntimeState decoded_runtime;
    DisplayWeightValue display_value;
    int64_t unbounded_count = 0;
    uint8_t bytes[PERSISTENT_V2_PAYLOAD_SIZE];
    uint16_t length = 0U;

    DefaultConfig_Load(&config);
    config.metrology.capacity_ug = INT64_C(1000000000);
    config.metrology.zero_range_ug = INT64_C(1000000000);
    config.metrology.overload_threshold_ug = INT64_C(10000000000);
    config.metrology.auto_zero_tracking_range_ug = INT64_C(1100000000);
    config.metrology.active_unit = MASS_UNIT_G;
    config.metrology.unit_display[MASS_UNIT_G].decimal_places = 2U;
    config.metrology.unit_display[MASS_UNIT_G].division_digit = 1U;
    CHECK(MetrologyConfig_ValidateCanonical(&config.metrology) ==
          METROLOGY_CONFIG_OK);
    CHECK(UnitConverter_MassToDisplay(INT64_C(10000000000), MASS_UNIT_G,
        &config.metrology.unit_display[MASS_UNIT_G], &display_value));
    CHECK(display_value.overflow && !display_value.valid);
    CHECK(UnitConverter_MassToCountUnbounded(INT64_C(10000000000),
        MASS_UNIT_G, 2U, 1U, &unbounded_count));
    CHECK(unbounded_count == INT64_C(1000000));
    CHECK(MetrologyLegacyProjection_Update(&config.metrology));
    CHECK(config.metrology.capacity == 100000U);
    CHECK(config.metrology.zero_range == 100000U);
    CHECK(config.metrology.overload_threshold == 1000000U);
    CHECK(config.metrology.auto_zero_tracking_range == 110000U);

    projection_only = config.metrology;
    projection_only.zero_range_ug = INT64_C(10000000000);
    projection_only.auto_zero_tracking_range_ug = INT64_C(10000000000);
    CHECK(MetrologyLegacyProjection_Update(&projection_only));
    CHECK(projection_only.zero_range == 1000000U);
    CHECK(projection_only.auto_zero_tracking_range == 1000000U);

    projection_only = config.metrology;
    projection_only.overload_threshold_ug = INT64_C(1100000000);
    CHECK(MetrologyLegacyProjection_Update(&projection_only));
    CHECK(projection_only.overload_threshold == 110000U);

    CHECK(CalibrationModel_BuildMass(0, 100000,
        INT64_C(10000000000), 1U, &config.calibration) ==
        CALIBRATION_RESULT_OK);
    CHECK(CalibrationLegacyProjection_Update(&config.calibration,
        MASS_UNIT_G, &config.metrology.unit_display[MASS_UNIT_G]));
    CHECK(config.calibration.span_weight == 1000000U);
    runtime.tare_active = true;
    runtime.current_tare_ug = INT64_C(10000000000);
    config.system.tare_power_loss_retention = true;
    CHECK(RuntimeLegacyProjection_Update(&runtime, MASS_UNIT_G,
        &config.metrology.unit_display[MASS_UNIT_G]));
    CHECK(runtime.current_tare == 1000000);

    CHECK(PersistentCodec_EncodeV2(&config, &runtime, bytes, sizeof(bytes),
        &length) == PERSISTENT_CODEC_OK);
    CHECK(PersistentCodec_DecodeV2(bytes, length, &decoded,
        &decoded_runtime) == PERSISTENT_CODEC_OK);
    CHECK(decoded.metrology.capacity_ug == INT64_C(1000000000));
    CHECK(decoded.metrology.overload_threshold_ug == INT64_C(10000000000));
    CHECK(decoded.calibration.span_mass_ug == INT64_C(10000000000));
    CHECK(decoded_runtime.current_tare_ug == INT64_C(10000000000));

    config.metrology.capacity_ug = INT64_C(10000000000);
    CHECK(MetrologyConfig_ValidateCanonical(&config.metrology) ==
          METROLOGY_CONFIG_INVALID_UNIT);
}

static void TestAlarmLegacyProjection(void)
{
    DeviceConfig config;
    DeviceConfig decoded;
    RuntimeState runtime = {0};
    RuntimeState decoded_runtime;
    uint8_t bytes[PERSISTENT_V2_PAYLOAD_SIZE];
    uint16_t length = 0U;

    DefaultConfig_Load(&config);
    config.alarm.limit_function_enable = true;
    config.alarm.lower_limit_ug = INT64_C(499000000);
    config.alarm.upper_limit_ug = INT64_C(501000000);
    config.alarm.hysteresis_ug = INT64_C(200000);
    config.alarm.lower_limit = 0;
    config.alarm.upper_limit = 0;
    config.alarm.hysteresis = 0U;
    runtime.weight_view = WEIGHT_VIEW_NET;

    CHECK(PersistentCodec_EncodeV2(&config, &runtime, bytes, sizeof(bytes),
        &length) == PERSISTENT_CODEC_OK);
    CHECK(length == PERSISTENT_V2_PAYLOAD_SIZE);
    CHECK(PersistentCodec_DecodeV2(bytes, length, &decoded,
        &decoded_runtime) == PERSISTENT_CODEC_OK);
    CHECK(decoded.alarm.lower_limit == 49900);
    CHECK(decoded.alarm.upper_limit == 50100);
    CHECK(decoded.alarm.hysteresis == 20U);
    CHECK(decoded.alarm.lower_limit_ug == INT64_C(499000000));
    CHECK(decoded.alarm.upper_limit_ug == INT64_C(501000000));
    CHECK(decoded.alarm.hysteresis_ug == INT64_C(200000));

    config.alarm.lower_limit_ug = INT64_C(-1000000);
    config.alarm.upper_limit_ug = INT64_C(1000000);
    config.alarm.hysteresis_ug = INT64_C(200000);
    CHECK(AlarmLegacyProjection_Update(&config.alarm, MASS_UNIT_G,
        &config.metrology.unit_display[MASS_UNIT_G]));
    CHECK(config.alarm.lower_limit == -100);
    CHECK(config.alarm.upper_limit == 100);
    CHECK(config.alarm.hysteresis == 20U);
}

static void TestCodec(void)
{
    DeviceConfig config,decoded;
    RuntimeState runtime={0},decoded_runtime;
    uint8_t bytes[PERSISTENT_V2_PAYLOAD_SIZE];
    uint8_t roundtrip[PERSISTENT_V2_PAYLOAD_SIZE];
    uint16_t length=0U;
    uint16_t roundtrip_length=0U;
    PersistentCodecResult result;
    DefaultConfig_Load(&config);
    config.system.startup_auto_zero_enable=true;
    runtime.weight_view=WEIGHT_VIEW_NET;
    result=PersistentCodec_EncodeV2(&config,&runtime,bytes,sizeof(bytes),&length);
    CHECK(result==PERSISTENT_CODEC_OK);
    CHECK(length==PERSISTENT_V2_PAYLOAD_SIZE);
    if(result==PERSISTENT_CODEC_OK)
    {
        CHECK(PersistentCodec_DecodeV2(bytes,length,&decoded,&decoded_runtime)==PERSISTENT_CODEC_OK);
        CHECK(PersistentCodec_EncodeV2(&decoded,&decoded_runtime,roundtrip,
            sizeof(roundtrip),&roundtrip_length)==PERSISTENT_CODEC_OK);
        CHECK(roundtrip_length==length&&memcmp(bytes,roundtrip,length)==0);
        CHECK(!decoded_runtime.migration_pending_save);
        CHECK(decoded.system.startup_auto_zero_enable);
    }
    result=PersistentCodec_EncodeV1(&config,&runtime,bytes,sizeof(bytes),&length);
    CHECK(result==PERSISTENT_CODEC_OK&&length==PERSISTENT_V1_PAYLOAD_SIZE);
    CHECK(PersistentCodec_MigrateV1ToV2(bytes,length,&decoded,&decoded_runtime)==PERSISTENT_CODEC_OK);
    CHECK(decoded_runtime.migration_pending_save&&decoded_runtime.config_dirty);
    CHECK(decoded.metrology.capacity_ug==INT64_C(3000000000));

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
    CHECK(PersistentCodec_EncodeV2(&config,&runtime,bytes,sizeof(bytes),&length)==PERSISTENT_CODEC_OK);
    CHECK(PersistentCodec_DecodeV2(bytes,length,&decoded,&decoded_runtime)==PERSISTENT_CODEC_OK);
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

static void TestLegacyDevelopmentNormalization(void)
{
    DeviceConfig config;
    DeviceConfig unchanged;
    CalibrationConfig calibration;
    CommunicationConfig communication;
    RuntimeState runtime={0};
    uint32_t flags;

    DefaultConfig_Load(&config);
    config.metrology.capacity_ug=INT64_C(2500000000);
    config.metrology.overload_threshold_ug=INT64_C(2600000000);
    config.metrology.zero_range_ug=0;
    config.metrology.profiles[0].stability_enter_threshold_ug=INT64_C(2000000);
    config.metrology.profiles[0].stability_exit_threshold_ug=INT64_C(4000000);
    config.metrology.profiles[0].stability_hold_ms=500U;
    config.calibration.raw_zero=123;
    config.calibration.raw_span=456;
    config.calibration.span_mass_ug=INT64_C(500000000);
    calibration=config.calibration;
    communication=config.communication;
    unchanged=config;
    flags=DefaultConfig_NormalizeLegacyDevelopment(&config);
    CHECK(flags==(DEFAULT_CONFIG_NORMALIZED_STABILITY|
        DEFAULT_CONFIG_NORMALIZED_ZERO_RANGE));
    CHECK(config.metrology.capacity_ug==INT64_C(2500000000));
    CHECK(config.metrology.overload_threshold_ug==INT64_C(2600000000));
    CHECK(config.metrology.zero_range_ug==INT64_C(60000000));
    CHECK(config.metrology.profiles[0].stability_enter_threshold_ug==INT64_C(50000));
    CHECK(config.metrology.profiles[0].stability_exit_threshold_ug==INT64_C(100000));
    CHECK(config.metrology.profiles[0].stability_hold_ms==1000U);
    CHECK(memcmp(&config.calibration,&calibration,sizeof(calibration))==0);
    CHECK(memcmp(&config.communication,&communication,sizeof(communication))==0);

    DefaultConfig_Load(&config);
    config.metrology.overload_threshold_ug=INT64_C(10000000000);
    calibration=config.calibration;
    communication=config.communication;
    flags=DefaultConfig_NormalizeLegacyDevelopment(&config);
    CHECK(flags==DEFAULT_CONFIG_NORMALIZED_OVERLOAD);
    CHECK(config.metrology.capacity_ug==INT64_C(3000000000));
    CHECK(config.metrology.overload_threshold_ug==INT64_C(3000000000));
    CHECK(memcmp(&config.calibration,&calibration,sizeof(calibration))==0);
    CHECK(memcmp(&config.communication,&communication,sizeof(communication))==0);
    CHECK(DefaultConfig_NormalizeLegacyDevelopment(&config)==
        DEFAULT_CONFIG_NORMALIZED_NONE);

    DefaultConfig_Load(&config);
    config.metrology.overload_threshold_ug=INT64_C(10000000000);
    config.metrology.profiles[0].filter_strength=2U;
    unchanged=config;
    CHECK(DefaultConfig_NormalizeLegacyDevelopment(&config)==
        DEFAULT_CONFIG_NORMALIZED_NONE);
    CHECK(memcmp(&config,&unchanged,sizeof(config))==0);

    DefaultConfig_Load(&config);
    config.metrology.capacity_ug=INT64_C(2500000000);
    config.metrology.overload_threshold_ug=INT64_C(2600000000);
    config.metrology.zero_range_ug=0;
    config.metrology.profiles[0].stability_enter_threshold_ug=INT64_C(2000000);
    config.metrology.profiles[0].stability_exit_threshold_ug=INT64_C(4000000);
    config.metrology.profiles[0].stability_hold_ms=500U;
    config.metrology.profiles[0].filter_strength=3U;
    CHECK(DefaultConfig_NormalizeStartup(&config,&runtime)==
        (DEFAULT_CONFIG_NORMALIZED_STABILITY|
         DEFAULT_CONFIG_NORMALIZED_ZERO_RANGE));
    CHECK(runtime.migration_pending_save&&runtime.config_dirty);
    CHECK(DefaultConfig_GetLastNormalizationFlags()==
        (DEFAULT_CONFIG_NORMALIZED_STABILITY|
         DEFAULT_CONFIG_NORMALIZED_ZERO_RANGE));

    config=unchanged;
    config.metrology.profiles[0].filter_strength=2U;
    unchanged=config;
    CHECK(DefaultConfig_NormalizeLegacyDevelopment(&config)==
        DEFAULT_CONFIG_NORMALIZED_NONE);
    CHECK(memcmp(&config,&unchanged,sizeof(config))==0);
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

int main(void)
{
    TestMassAndUnits(); TestCanonicalAndLegacyBoundary();
    TestUnboundedLegacyProjection(); TestAlarmLegacyProjection(); TestCodec();
    TestReferenceRules(); TestProductDefaults();
    TestLegacyDevelopmentNormalization(); TestKeyConflict(); TestModbusModel();
    if(failures==0U) printf("Stage 5A host tests passed.\n");
    return failures==0U?0:1;
}
