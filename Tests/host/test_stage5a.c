#include "calibration_model.h"
#include "default_config.h"
#include "key_service.h"
#include "mass_math.h"
#include "metrology_standard_validator.h"
#include "persistent_codec.h"
#include "persistent_schema.h"
#include "unit_converter.h"
#include "modbus_register_model.h"
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
    }
    result=PersistentCodec_EncodeV1(&config,&runtime,bytes,sizeof(bytes),&length);
    CHECK(result==PERSISTENT_CODEC_OK&&length==PERSISTENT_V1_PAYLOAD_SIZE);
    CHECK(PersistentCodec_MigrateV1ToV2(bytes,length,&decoded,&decoded_runtime)==PERSISTENT_CODEC_OK);
    CHECK(decoded_runtime.migration_pending_save&&decoded_runtime.config_dirty);
    CHECK(decoded.metrology.capacity_ug==INT64_C(10000000000));

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
    config.metrology.compliance_mode=METROLOGY_COMPLIANCE_CLASS_III_REFERENCE;
    CHECK(MetrologyStandardValidator_Validate(&config.metrology)==METROLOGY_STANDARD_OK);
    CHECK(MetrologyStandardValidator_GetMinimumLoad(&config.metrology)==INT64_C(20000000));
    CHECK(MetrologyStandardValidator_GetDisplayOverload(&config.metrology)==INT64_C(10009000000));
    config.metrology.active_unit=MASS_UNIT_LB;
    CHECK(MetrologyStandardValidator_Validate(&config.metrology)==METROLOGY_STANDARD_INVALID_UNIT);
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
    uint16_t request[]={1U,1U,0U,0U,0U,0U,0U,0U,0U,0U,0U,0xA55AU};
    Stage5A_ModelAdaptersInit();
    Stage5A_ModelSnapshot()->net_mass_ug=INT64_C(0x1122334455667788);
    ModbusRegisterModel_Init();
    CHECK(ModbusRegisterModel_ReadHolding(0x0010U,4U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==0x1122U&&words[1]==0x3344U&&words[2]==0x5566U&&words[3]==0x7788U);
    Stage5A_ModelContext()->config.communication.word_order=MODBUS_WORD_ORDER_LOW_WORD_FIRST;
    CHECK(ModbusRegisterModel_ReadHolding(0x0010U,4U,words)==MODBUS_REGISTER_OK);
    CHECK(words[0]==0x7788U&&words[1]==0x5566U&&words[2]==0x3344U&&words[3]==0x1122U);
    CHECK(ModbusRegisterModel_WriteMultiple(0x0040U,12U,request)==MODBUS_REGISTER_OK);
    CHECK(Stage5A_ModelCommandCount()==1U);
    CHECK(ModbusRegisterModel_WriteSingle(0x004BU,0xA55AU)==MODBUS_REGISTER_OK);
    CHECK(Stage5A_ModelCommandCount()==1U);
    CHECK(ModbusRegisterModel_WriteSingle(0x0100U,1U)==MODBUS_REGISTER_READ_ONLY);
    CHECK(ModbusRegisterModel_WriteMultiple(0x0140U,2U,words)==MODBUS_REGISTER_OK);
    CHECK(ModbusRegisterModel_ReadHolding(0x017FU,1U,words)==MODBUS_REGISTER_OK&&words[0]==1U);
    words[0]=2U; words[1]=5U;
    CHECK(ModbusRegisterModel_WriteMultiple(0x01A1U,2U,words)==MODBUS_REGISTER_ILLEGAL_VALUE);
    CHECK(ModbusRegisterModel_ReadHolding(0x01A1U,1U,words)==MODBUS_REGISTER_OK&&words[0]==1U);
}

int main(void)
{
    TestMassAndUnits(); TestCodec(); TestReferenceRules(); TestKeyConflict(); TestModbusModel();
    if(failures==0U) printf("Stage 5A host tests passed.\n");
    return failures==0U?0:1;
}
