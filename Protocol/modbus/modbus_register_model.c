#include "modbus_register_model.h"

#include "calibration_model.h"
#include "command_service.h"
#include "config_store.h"
#include "cs1237.h"
#include "fault_manager.h"
#include "key_service.h"
#include "metrology_config_validator.h"
#include "metrology_manager.h"
#include "modbus_command_mailbox.h"
#include "modbus_register_map.h"
#include "persistent_schema.h"
#include "storage_power_guard.h"
#include "system_context.h"
#include "unit_converter.h"

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#define STAGING_REGISTER_COUNT 64U

static uint16_t s_staging[STAGING_REGISTER_COUNT];
static uint16_t s_staging_validation;
static bool s_staging_dirty;
static CommunicationConfig s_pending_communication;
static ModbusRegisterResult ReadActive(uint16_t address,
    const DeviceConfig *config, uint16_t *value);

static uint64_t Join64(const uint16_t *words, ModbusWordOrder order)
{
    uint8_t i; uint64_t result=0U;
    for(i=0U;i<4U;++i)
    {
        uint8_t source=(order==MODBUS_WORD_ORDER_HIGH_WORD_FIRST)?i:(uint8_t)(3U-i);
        result=(result<<16U)|words[source];
    }
    return result;
}

static void RefreshStaging(const DeviceConfig *config)
{
    uint16_t i,value;
    for(i=0U;i<STAGING_REGISTER_COUNT;++i)
    { (void)ReadActive((uint16_t)(0x0100U+i),config,&value); s_staging[i]=value; }
    s_staging_dirty=false; s_staging_validation=0U;
}

static bool DecodeStaging(const DeviceConfig *active,DeviceConfig *candidate)
{
    uint8_t p; ModbusWordOrder order=active->communication.word_order;
    if(candidate==NULL)return false;
    *candidate=*active;
    candidate->metrology.compliance_mode=(MetrologyComplianceMode)s_staging[0];
    candidate->metrology.active_unit=(MassUnit)s_staging[1];
    candidate->metrology.enabled_unit_mask=(uint8_t)s_staging[2];
    candidate->communication.word_order=(ModbusWordOrder)s_staging[3];
    candidate->metrology.capacity_ug=(MassValueUg)Join64(&s_staging[4],order);
    candidate->metrology.verification_interval_e_ug=(MassValueUg)Join64(&s_staging[8],order);
    candidate->metrology.initial_zero_range_permille=s_staging[12];
    candidate->metrology.semi_auto_zero_range_permille=s_staging[13];
    candidate->metrology.auto_zero_tracking_enable=s_staging[14]!=0U;
    candidate->system.tare_power_loss_retention=s_staging[15]!=0U;
    for(p=0U;p<MASS_UNIT_COUNT;++p)
    {
        candidate->metrology.unit_display[p].decimal_places=(uint8_t)s_staging[16U+p*2U];
        candidate->metrology.unit_display[p].division_digit=(uint8_t)s_staging[17U+p*2U];
        candidate->metrology.unit_display[p].enabled=
            (candidate->metrology.enabled_unit_mask&(uint8_t)(1U<<p))!=0U;
    }
    candidate->display.brightness=(uint8_t)s_staging[22];
    candidate->metrology.load_cell.rated_capacity_known=(s_staging[23]&1U)!=0U;
    candidate->metrology.load_cell.sensitivity_known=(s_staging[23]&2U)!=0U;
    candidate->metrology.load_cell.safe_load_known=(s_staging[23]&4U)!=0U;
    candidate->metrology.load_cell.rated_capacity_ug=(MassValueUg)Join64(&s_staging[24],order);
    candidate->metrology.load_cell.sensitivity_uv_per_v=
        ((uint32_t)s_staging[28]<<16U)|s_staging[29];
    candidate->metrology.load_cell.safe_load_permille=s_staging[30];
    candidate->metrology.active_profile=(WeighingProfileId)s_staging[31];
    for(p=0U;p<WEIGHING_PROFILE_COUNT;++p)
    {
        uint8_t base=(p==0U)?32U:46U;
        WeighingProfileConfig *profile=&candidate->metrology.profiles[p];
        profile->sample_rate=(Cs1237DataRate)s_staging[base];
        profile->gain=(Cs1237Gain)s_staging[base+1U];
        profile->filter_mode=(FilterMode)s_staging[base+2U];
        profile->filter_strength=(uint8_t)s_staging[base+3U];
        profile->stability_window=(uint8_t)s_staging[base+4U];
        profile->stability_hold_ms=s_staging[base+5U];
        profile->stability_enter_threshold_ug=(MassValueUg)Join64(&s_staging[base+6U],order);
        profile->stability_exit_threshold_ug=(MassValueUg)Join64(&s_staging[base+10U],order);
    }
    if ((uint32_t)candidate->metrology.active_unit>=MASS_UNIT_COUNT)return false;
    candidate->metrology.unit=candidate->metrology.active_unit;
    candidate->metrology.decimal_places=candidate->metrology.unit_display[candidate->metrology.active_unit].decimal_places;
    candidate->metrology.division=candidate->metrology.unit_display[candidate->metrology.active_unit].division_digit;
    return true;
}

static uint16_t Word32(uint32_t value, uint8_t index, ModbusWordOrder order)
{
    uint8_t selected = (order == MODBUS_WORD_ORDER_HIGH_WORD_FIRST) ? index :
        (uint8_t)(1U - index);
    return (selected == 0U) ? (uint16_t)(value >> 16U) : (uint16_t)value;
}

static uint16_t Word64(uint64_t value, uint8_t index, ModbusWordOrder order)
{
    uint8_t selected = (order == MODBUS_WORD_ORDER_HIGH_WORD_FIRST) ? index :
        (uint8_t)(3U - index);
    return (uint16_t)(value >> ((3U - selected) * 16U));
}

static uint16_t BaudEnum(uint32_t baud)
{
    if (baud == 9600U) return 0U;
    if (baud == 19200U) return 1U;
    if (baud == 38400U) return 2U;
    if (baud == 57600U) return 3U;
    return (baud == 115200U) ? 4U : 0xFFFFU;
}

static ModbusRegisterResult ReadActive(uint16_t address,
    const DeviceConfig *config, uint16_t *value)
{
    const MetrologyConfig *m = &config->metrology;
    uint8_t offset;
    *value = 0U;
    switch (address)
    {
        case 0x0100U:*value=(uint16_t)m->compliance_mode; break;
        case 0x0101U:*value=(uint16_t)m->active_unit; break;
        case 0x0102U:*value=m->enabled_unit_mask; break;
        case 0x0103U:*value=(uint16_t)config->communication.word_order; break;
        case 0x010CU:*value=m->initial_zero_range_permille; break;
        case 0x010DU:*value=m->semi_auto_zero_range_permille; break;
        case 0x010EU:*value=m->auto_zero_tracking_enable ? 1U:0U; break;
        case 0x010FU:*value=config->system.tare_power_loss_retention?1U:0U; break;
        case 0x0110U: case 0x0112U: case 0x0114U:
            *value=m->unit_display[(address-0x0110U)/2U].decimal_places; break;
        case 0x0111U: case 0x0113U: case 0x0115U:
            *value=(uint16_t)m->unit_display[(address-0x0111U)/2U].division_digit; break;
        case 0x0116U:*value=config->display.brightness; break;
        case 0x0117U:*value=(m->load_cell.rated_capacity_known?1U:0U)|
            (m->load_cell.sensitivity_known?2U:0U)|(m->load_cell.safe_load_known?4U:0U); break;
        case 0x011EU:*value=m->load_cell.safe_load_permille; break;
        case 0x011FU:*value=(uint16_t)m->active_profile; break;
        case 0x0120U: case 0x012EU:
            *value=(uint16_t)m->profiles[(address==0x0120U)?0:1].sample_rate; break;
        case 0x0121U: case 0x012FU:
            *value=(uint16_t)m->profiles[(address==0x0121U)?0:1].gain; break;
        case 0x0122U: case 0x0130U:
            *value=(uint16_t)m->profiles[(address==0x0122U)?0:1].filter_mode; break;
        case 0x0123U: case 0x0131U:
            *value=m->profiles[(address==0x0123U)?0:1].filter_strength; break;
        case 0x0124U: case 0x0132U:
            *value=m->profiles[(address==0x0124U)?0:1].stability_window; break;
        case 0x0125U: case 0x0133U:
            *value=(uint16_t)m->profiles[(address==0x0125U)?0:1].stability_hold_ms; break;
        case 0x013CU:*value=0U; break;
        case 0x013EU:*value=CONFIG_STORE_SCHEMA_V2; break;
        case 0x013FU:*value=(uint16_t)MetrologyConfig_Validate(m,&config->stability); break;
        default:
            if ((address>=0x0104U)&&(address<=0x0107U))
                *value=Word64((uint64_t)m->capacity_ug,(uint8_t)(address-0x0104U),config->communication.word_order);
            else if ((address>=0x0108U)&&(address<=0x010BU))
                *value=Word64((uint64_t)m->verification_interval_e_ug,(uint8_t)(address-0x0108U),config->communication.word_order);
            else if ((address>=0x0118U)&&(address<=0x011BU))
                *value=Word64((uint64_t)m->load_cell.rated_capacity_ug,(uint8_t)(address-0x0118U),config->communication.word_order);
            else if ((address>=0x011CU)&&(address<=0x011DU))
                *value=Word32(m->load_cell.sensitivity_uv_per_v,(uint8_t)(address-0x011CU),config->communication.word_order);
            else if (((address>=0x0126U)&&(address<=0x012DU)) ||
                     ((address>=0x0134U)&&(address<=0x013BU)))
            {
                uint8_t profile=(address>=0x0134U)?1U:0U;
                uint16_t base=profile?0x0134U:0x0126U;
                offset=(uint8_t)(address-base);
                *value=Word64((uint64_t)((offset<4U)?m->profiles[profile].stability_enter_threshold_ug:m->profiles[profile].stability_exit_threshold_ug),
                    (uint8_t)(offset%4U),config->communication.word_order);
            }
            break;
    }
    return MODBUS_REGISTER_OK;
}

static ModbusRegisterResult ReadOne(uint16_t address,
    const SystemContext *context, const MassSnapshot *snapshot, uint16_t *value)
{
    const DeviceConfig *config=&context->config;
    ModbusWordOrder order=config->communication.word_order;
    DisplayWeightValue net={0},gross={0},tare={0};
    const UnitDisplayConfig *display=&config->metrology.unit_display[config->metrology.active_unit];
    uint32_t flags=(snapshot!=NULL)?snapshot->status_flags:0U;
    (void)UnitConverter_MassToDisplay((snapshot!=NULL)?snapshot->net_mass_ug:0,config->metrology.active_unit,display,&net);
    (void)UnitConverter_MassToDisplay((snapshot!=NULL)?snapshot->gross_mass_ug:0,config->metrology.active_unit,display,&gross);
    (void)UnitConverter_MassToDisplay((snapshot!=NULL)?snapshot->tare_mass_ug:0,config->metrology.active_unit,display,&tare);
    *value=0U;
    if ((address>=MODBUS_MAILBOX_FIRST)&&(address<=MODBUS_MAILBOX_LAST))
        return ModbusCommandMailbox_Read(address,value);
    if ((address>=MODBUS_ACTIVE_CONFIG_FIRST)&&(address<=MODBUS_ACTIVE_CONFIG_LAST))
        return ReadActive(address,config,value);
    if ((address>=MODBUS_STAGING_CONFIG_FIRST)&&(address<=MODBUS_STAGING_CONFIG_LAST))
    { *value=(address==0x017EU)?s_staging_validation:((address==0x017FU)?(s_staging_dirty?1U:0U):s_staging[address-0x0140U]); return MODBUS_REGISTER_OK; }
    if (address<=0x001FU)
    {
        int32_t page=(context->runtime.weight_view==WEIGHT_VIEW_GROSS)?gross.display_count:net.display_count;
        if(address<=1U)*value=Word32((uint32_t)page,(uint8_t)address,order);
        else if(address==2U)*value=display->decimal_places;
        else if(address==3U)*value=(uint16_t)config->metrology.active_unit;
        else if(address==4U)*value=(uint16_t)flags;
        else if(address==5U)*value=(uint16_t)(flags>>16U);
        else if(address>=6U&&address<=7U)*value=Word32((uint32_t)net.display_count,(uint8_t)(address-6U),order);
        else if(address>=8U&&address<=9U)*value=Word32((uint32_t)gross.display_count,(uint8_t)(address-8U),order);
        else if(address>=10U&&address<=11U)*value=Word32((uint32_t)tare.display_count,(uint8_t)(address-10U),order);
        else if(address==12U)*value=(uint16_t)display->division_digit;
        else if(address==13U)*value=(uint16_t)context->runtime.weight_view;
        else if(address==14U)*value=MODBUS_REGISTER_MAP_VERSION;
        else if(address==15U)*value=0x050AU;
        else if(address>=0x10U&&address<=0x13U)*value=Word64((uint64_t)((snapshot!=NULL)?snapshot->net_mass_ug:0),(uint8_t)(address-0x10U),order);
        else if(address>=0x14U&&address<=0x17U)*value=Word64((uint64_t)((snapshot!=NULL)?snapshot->gross_mass_ug:0),(uint8_t)(address-0x14U),order);
        else if(address>=0x18U&&address<=0x1BU)*value=Word64((uint64_t)((snapshot!=NULL)?snapshot->tare_mass_ug:0),(uint8_t)(address-0x18U),order);
        else if(address>=0x1CU&&address<=0x1DU)*value=Word32((uint32_t)((snapshot!=NULL)?snapshot->raw_value:0),(uint8_t)(address-0x1CU),order);
        else *value=Word32((uint32_t)((snapshot!=NULL)?snapshot->filtered_raw:0),(uint8_t)(address-0x1EU),order);
        return MODBUS_REGISTER_OK;
    }
    if ((address>=0x0020U)&&(address<=0x003FU))
    {
        uint32_t sequence=(snapshot!=NULL)?snapshot->sample_sequence:0U;
        uint32_t stamp=(snapshot!=NULL)?snapshot->sample_timestamp_ms:0U;
        uint64_t spread=(snapshot!=NULL)?snapshot->stability_spread_ug:0U;
        if(address<=0x21U)*value=Word32(sequence,(uint8_t)(address-0x20U),order);
        else if(address<=0x23U)*value=Word32(stamp,(uint8_t)(address-0x22U),order);
        else if(address<=0x27U)*value=Word64(spread,(uint8_t)(address-0x24U),order);
        else if(address==0x28U)*value=(uint16_t)config->metrology.active_profile;
        else if(address==0x29U)*value=(uint16_t)config->metrology.profiles[config->metrology.active_profile].sample_rate;
        else if(address==0x2AU)*value=(uint16_t)config->metrology.profiles[config->metrology.active_profile].gain;
        else if(address==0x2BU)*value=(uint16_t)CS1237_GetState();
        else if(address==0x2CU)*value=CS1237_GetBufferedSampleCount();
        else if(address<=0x2EU)*value=Word32(CS1237_GetBufferOverrunCount(),(uint8_t)(address-0x2DU),order);
        else if(address==0x30U)*value=(uint16_t)ConfigStore_GetState();
        else if(address==0x31U)*value=StoragePowerGuard_CanContinueFlashOperation()?1U:0U;
        else if(address==0x32U)*value=context->runtime.config_dirty?1U:0U;
        else if(address<=0x34U)*value=Word32(context->config_revision,(uint8_t)(address-0x33U),order);
        else if(address<=0x36U)*value=Word32(context->saved_revision,(uint8_t)(address-0x35U),order);
        else if(address==0x37U)*value=KeyService_GetLastRawMask();
        else if(address==0x38U)*value=KeyService_GetLastLogicalMask();
        else if(address<=0x3AU)*value=Word32(FaultManager_GetActiveMask(),(uint8_t)(address-0x39U),order);
        else if(address==0x3BU)*value=config->calibration.calibration_valid?1U:0U;
        else if(address==0x3DU)*value=(uint16_t)CalibrationModel_GetSensorDirection(&config->calibration);
        else if(address==0x3EU)*value=(uint16_t)KeyService_GetMultiKeyConflictCount();
        else if(address==0x3FU)*value=ModbusCommandMailbox_GetLastResult();
        return MODBUS_REGISTER_OK;
    }
    if ((address>=0x0180U)&&(address<=0x019FU))
    {
        if(address>=0x0190U&&address<=0x0191U)*value=Word32((uint32_t)config->calibration.raw_zero,(uint8_t)(address-0x0190U),order);
        else if(address>=0x0192U&&address<=0x0193U)*value=Word32((uint32_t)config->calibration.raw_span,(uint8_t)(address-0x0192U),order);
        else if(address>=0x0194U&&address<=0x0197U)*value=Word64((uint64_t)config->calibration.span_mass_ug,(uint8_t)(address-0x0194U),order);
        else if(address>=0x0198U&&address<=0x0199U)*value=Word32(config->calibration.calibration_sequence,(uint8_t)(address-0x0198U),order);
        return MODBUS_REGISTER_OK;
    }
    if ((address>=0x01A0U)&&(address<=0x01BFU))
    {
        const CommunicationConfig *communication=&s_pending_communication;
        if(address==0x01A0U)*value=(uint16_t)communication->protocol_mode;
        else if(address==0x01A1U)*value=communication->modbus_address;
        else if(address==0x01A2U)*value=BaudEnum(communication->baud_rate);
        else if(address==0x01A3U)*value=(communication->parity==COMM_PARITY_ODD)?1U:((communication->parity==COMM_PARITY_EVEN)?2U:0U);
        else if(address==0x01A4U)*value=(communication->stop_bits==COMM_STOP_BITS_2)?2U:1U;
        else if(address==0x01A5U)*value=(uint16_t)communication->word_order;
        else if(address==0x01A6U)*value=communication->response_delay_ms;
        else if(address==0x01A7U)*value=communication->recommended_poll_interval_ms;
        else if(address==0x01A8U)*value=communication->broadcast_write_policy;
        else if(address==0x01A9U)*value=communication->pending_apply?1U:0U;
        return MODBUS_REGISTER_OK;
    }
    if ((address>=0x01C0U)&&(address<=0x01DFU))
    {
        if(address==0x01C0U)*value=CONFIG_STORE_SCHEMA_V2;
        else if(address==0x01C1U)*value=ConfigStore_GetActiveSlot();
        else if(address>=0x01C2U&&address<=0x01C3U)*value=Word32(ConfigStore_GetActiveSequence(),(uint8_t)(address-0x01C2U),order);
        else if(address==0x01C4U)*value=(uint16_t)ConfigStore_GetState();
        return MODBUS_REGISTER_OK;
    }
    return MODBUS_REGISTER_ILLEGAL_ADDRESS;
}

void ModbusRegisterModel_Init(void)
{
    const SystemContext *context=SystemContext_Get();
    ModbusCommandMailbox_Init(); s_staging_dirty=false; s_staging_validation=0U;
    if(context!=NULL)
    {
        RefreshStaging(&context->config);
        s_pending_communication=context->config.communication;
    }
}

ModbusRegisterResult ModbusRegisterModel_ReadHolding(uint16_t start_address,
    uint16_t count,uint16_t *destination)
{
    const SystemContext *live=SystemContext_Get(); SystemContext copy;
    const MassSnapshot *live_snapshot=MetrologyManager_GetMassSnapshot(); MassSnapshot snapshot;
    uint16_t i; ModbusRegisterResult result;
    if((count==0U)||(destination==NULL)||(live==NULL)||
       ((uint32_t)start_address+count>0x10000UL)) return MODBUS_REGISTER_ILLEGAL_VALUE;
    copy=*live; if(live_snapshot!=NULL)snapshot=*live_snapshot; else (void)memset(&snapshot,0,sizeof(snapshot));
    for(i=0U;i<count;++i){result=ReadOne((uint16_t)(start_address+i),&copy,&snapshot,&destination[i]);if(result!=MODBUS_REGISTER_OK)return result;}
    return MODBUS_REGISTER_OK;
}

static bool CommunicationValueValid(uint16_t address,uint16_t value)
{
    if(address==0x01A1U)return (value>=1U)&&(value<=247U);
    if(address==0x01A2U)return value<5U;
    if(address==0x01A3U)return value<=2U;
    if(address==0x01A4U)return (value==1U)||(value==2U);
    if(address==0x01A5U)return value<MODBUS_WORD_ORDER_COUNT;
    if(address==0x01A7U)return value!=0U;
    return true;
}

static ModbusRegisterResult ValidateWriteAddress(uint16_t address,uint16_t value)
{
    if((address>=0x0140U)&&(address<=0x017DU))return MODBUS_REGISTER_OK;
    if((address>=0x0040U)&&(address<=0x004BU))
        return ((address==0x004BU)&&(value!=MODBUS_EXECUTE_VALUE))?MODBUS_REGISTER_ILLEGAL_VALUE:MODBUS_REGISTER_OK;
    if((address>=0x01A1U)&&(address<=0x01A8U))return
        CommunicationValueValid(address,value)?MODBUS_REGISTER_OK:
            MODBUS_REGISTER_ILLEGAL_VALUE;
    if(((address>=0x0000U)&&(address<=0x003FU))||
       ((address>=0x004CU)&&(address<=0x005FU))||
       ((address>=0x0100U)&&(address<=0x013FU))||
       ((address>=0x017EU)&&(address<=0x01DFU)))return MODBUS_REGISTER_READ_ONLY;
    return MODBUS_REGISTER_ILLEGAL_ADDRESS;
}

ModbusRegisterResult ModbusRegisterModel_WriteSingle(uint16_t address,uint16_t value)
{
    ModbusRegisterResult result=ValidateWriteAddress(address,value);
    const SystemContext *context;
    uint16_t command_id;
    DeviceConfig candidate;
    if(result!=MODBUS_REGISTER_OK)return result;
    if(address>=0x0140U&&address<=0x017DU){s_staging[address-0x0140U]=value;s_staging_dirty=true;s_staging_validation=0xFFFFU;return MODBUS_REGISTER_OK;}
    if(address<=0x004BU)
    {
        if(address==0x004BU)
        {
            context=SystemContext_Get();
            if((context==NULL)||(ModbusCommandMailbox_Read(0x0041U,&command_id)!=MODBUS_REGISTER_OK))
                return MODBUS_REGISTER_DEVICE_FAILURE;
            if(command_id==9U){RefreshStaging(&context->config);CommandService_ClearStagedConfig();}
            else if((command_id==10U)||(command_id==11U))
            {
                if(!DecodeStaging(&context->config,&candidate)||
                   !CommandService_SetStagedConfig(&candidate))return MODBUS_REGISTER_ILLEGAL_VALUE;
            }
            else if(command_id==12U)CommandService_ClearStagedConfig();
            result=ModbusCommandMailbox_Write(address,value);
            if(command_id==10U)s_staging_validation=ModbusCommandMailbox_GetLastResult();
            if((command_id==11U)&&(ModbusCommandMailbox_GetLastResult()==COMMAND_RESULT_OK))s_staging_dirty=false;
            return result;
        }
        return ModbusCommandMailbox_Write(address,value);
    }
    if(address>=0x01A1U&&address<=0x01A8U)
    {
        switch(address)
        {
            case 0x01A1U: s_pending_communication.modbus_address=(uint8_t)value; break;
            case 0x01A2U:
            { static const uint32_t baud[]={9600U,19200U,38400U,57600U,115200U}; s_pending_communication.baud_rate=baud[value]; break; }
            case 0x01A3U: s_pending_communication.parity=(value==1U)?COMM_PARITY_ODD:((value==2U)?COMM_PARITY_EVEN:COMM_PARITY_NONE); break;
            case 0x01A4U: s_pending_communication.stop_bits=(value==2U)?COMM_STOP_BITS_2:COMM_STOP_BITS_1; break;
            case 0x01A5U: s_pending_communication.word_order=(ModbusWordOrder)value; break;
            case 0x01A6U: s_pending_communication.response_delay_ms=value; break;
            case 0x01A7U: s_pending_communication.recommended_poll_interval_ms=value; break;
            case 0x01A8U: s_pending_communication.broadcast_write_policy=(uint8_t)value; break;
            default: break;
        }
        s_pending_communication.pending_apply=true;
    }
    return MODBUS_REGISTER_OK;
}

ModbusRegisterResult ModbusRegisterModel_WriteMultiple(uint16_t start_address,
    uint16_t count,const uint16_t *values)
{
    uint16_t i; ModbusRegisterResult result;
    if((count==0U)||(values==NULL)||((uint32_t)start_address+count>0x10000UL))return MODBUS_REGISTER_ILLEGAL_VALUE;
    for(i=0U;i<count;++i){result=ValidateWriteAddress((uint16_t)(start_address+i),values[i]);if(result!=MODBUS_REGISTER_OK)return result;
        if(((uint16_t)(start_address+i)==0x004BU)&&(i!=(uint16_t)(count-1U)))return MODBUS_REGISTER_ILLEGAL_VALUE;}
    for(i=0U;i<count;++i){result=ModbusRegisterModel_WriteSingle((uint16_t)(start_address+i),values[i]);if(result!=MODBUS_REGISTER_OK)return result;}
    return MODBUS_REGISTER_OK;
}
