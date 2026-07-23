#include "stage2a_driver_diagnostics.h"

#include "project_config.h"

#if (ENABLE_STAGE2A_DIAGNOSTICS != 0U)
#include "battery_adc.h"
#include "cs1237.h"
#include "device_manager.h"
#include "tm1628.h"

static bool s_active;
static CS1237_Sample s_latest_cs1237_sample;
static BatteryAdcState s_latest_battery_state;
static uint8_t s_latest_key_mask;
#endif

void Stage2A_DiagnosticsInit(void)
{
#if (ENABLE_STAGE2A_DIAGNOSTICS != 0U)
    s_active = true;
    TM1628_Clear();
    (void)TM1628_Flush();
#endif
}

void Stage2A_DiagnosticsProcess(void)
{
#if (ENABLE_STAGE2A_DIAGNOSTICS != 0U)
    const BatteryAdcState *battery;

    if (!s_active)
    {
        return;
    }

    (void)CS1237_TryPopSample(&s_latest_cs1237_sample);
    battery = BatteryAdc_GetState();
    if (battery != NULL)
    {
        s_latest_battery_state = *battery;
    }
    s_latest_key_mask = DeviceManager_GetLastRawKeyMask();
#endif
}
