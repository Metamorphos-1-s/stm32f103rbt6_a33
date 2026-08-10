#include "alarm_config_validation.h"
#include "calibration_model.h"
#include "config_edit.h"
#include "metrology_config_validator.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

MetrologyConfigResult MetrologyConfig_ValidateProductHardware(
    const MetrologyConfig *metrology)
{
    return (metrology != NULL) ? METROLOGY_CONFIG_OK : METROLOGY_CONFIG_NULL;
}

bool AlarmConfig_Validate(const AlarmConfig *config)
{
    return config != NULL;
}

CalibrationResult CalibrationModel_Validate(
    const CalibrationConfig *calibration)
{
    return (calibration != NULL) ? CALIBRATION_RESULT_OK :
                                   CALIBRATION_RESULT_NULL;
}

int main(void)
{
    DeviceConfig config;
    DeviceConfig applied;
    (void)memset(&config,0,sizeof(config));
    assert(ENABLE_STAGE5E_A3_LOCAL_MENU==0U);
    assert(CONFIG_FIELD_COUNT>CONFIG_FIELD_LIMIT_ENABLE);
    assert(CONFIG_MASS_FIELD_COUNT>CONFIG_MASS_FIELD_ALARM_LOWER_LIMIT);
    assert(ConfigEdit_Init());
    assert(ConfigEdit_Begin(&config));
    assert(ConfigEdit_SetMassField(CONFIG_MASS_FIELD_ALARM_LOWER_LIMIT,
                                   INT64_C(499000000)));
    assert(ConfigEdit_SetMassField(CONFIG_MASS_FIELD_ALARM_UPPER_LIMIT,
                                   INT64_C(501000000)));
    assert(ConfigEdit_SetMassField(CONFIG_MASS_FIELD_ALARM_HYSTERESIS,
                                   INT64_C(200000)));
    assert(ConfigEdit_SetIntegerField(CONFIG_FIELD_LIMIT_ENABLE,1));
    assert(ConfigEdit_SetIntegerField(CONFIG_FIELD_ALARM_WEIGHT_SOURCE,
                                      ALARM_WEIGHT_GROSS));
    assert(ConfigEdit_SetIntegerField(CONFIG_FIELD_INTERNAL_BUZZER_ENABLE,1));
    assert(ConfigEdit_SetIntegerField(CONFIG_FIELD_EXTERNAL_BUZZER_ENABLE,1));
    assert(ConfigEdit_SetIntegerField(CONFIG_FIELD_QUALIFIED_BEEP_ENABLE,1));
    assert(ConfigEdit_Validate());
    assert(ConfigEdit_CommitToRam(&applied));
    assert(applied.alarm.limit_function_enable);
    assert(applied.alarm.lower_limit_ug==INT64_C(499000000));
    assert(applied.alarm.weight_source==ALARM_WEIGHT_GROSS);
    puts("ConfigEdit alarm fields remain available without local menu");
    return 0;
}
