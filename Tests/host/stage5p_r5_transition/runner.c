/* Full 40 Hz host pipeline, not a device interface or an interpolated replay. */
#define _CRT_SECURE_NO_WARNINGS 1
#include "app_main.h"
#include "alarm_output_manager.h"
#include "calibration_model.h"
#include "command_service.h"
#include "default_config.h"
#include "display_controller.h"
#include "fault_manager.h"
#include "metrology_manager.h"
#include "mock_hal.h"
#include "system_context.h"

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static GuardedCheckweigh check;
bool App_GetAlarmOutputDiagnostics(AlarmOutputDiagnostics *out)
{
    if (out == NULL) return false;
    (void)memset(out, 0, sizeof(*out));
    return true;
}
bool App_A13CheckweighIsOff(void) { return check.mode == GUARDED_CHECKWEIGH_OFF; }
bool App_GetGuardedCheckweighState(GuardedCheckweigh *out)
{ if (out == NULL) return false; *out = check; return true; }
bool App_SetGuardedCheckweighMode(GuardedCheckweighMode mode, uint32_t generation, bool require)
{ return GuardedCheckweigh_SetMode(&check, mode, generation, require); }
bool App_RestoreGuardedCheckweighMode(GuardedCheckweighMode mode)
{ return GuardedCheckweigh_SetMode(&check, mode, 0U, false); }

int main(void)
{
    DeviceConfig config;
    uint32_t sequence = 0U;
    unsigned previous_mode = 2U;
    int32_t raw;
    unsigned mode;
    DefaultConfig_Load(&config);
    config.metrology.profiles[0].sample_rate = DEVICE_CS1237_DATA_RATE_40_HZ;
    config.metrology.profiles[0].filter_mode = FILTER_MODE_AVERAGE;
    config.metrology.profiles[0].filter_strength = 3U;
    config.system.requested_r5_application = 1U;
    config.system.requested_r5_mode = 2U;
    config.system.requested_checkweigh_mode = 1U;
    assert(CalibrationModel_BuildMass(41868, 485780, INT64_C(500000000),
        1U, &config.calibration) == CALIBRATION_RESULT_OK);
    TestMock_Reset();
    FaultManager_Init();
    GuardedCheckweigh_Init(&check);
    assert(SystemContext_Init(&config, 23U));
    assert(MetrologyManager_Init(&config, &SystemContext_Get()->runtime));
    assert(SystemContext_SetState(APP_STATE_RUN, 0U));
    assert(DisplayController_Init());
    CommandService_Init();
    assert(MetrologyManager_RestoreR5Request(R5_BETA_APPLICATION_ACTIVE,
        R5_DRIFT_MODE_STATIC_COMPENSATION));
    puts("sequence,uncompensated_ug,net_ug,offset_ug,state,reason,reference_twice_ug,reference_lock_sequence,first_correction_sequence,limited,application");
    while (scanf("%" SCNd32 ",%u", &raw, &mode) == 2) {
        RawMeasurementSample sample;
        const WeightSnapshot *weight;
        const A13CSnapshot *r5;
        if (mode != 1U && mode != 2U) return 2;
        if (mode != previous_mode) {
            if (!MetrologyManager_SetA13Pair(R5_BETA_APPLICATION_ACTIVE,
                    (R5DriftMode)mode, 0U, false)) return 3;
            previous_mode = mode;
        }
        sample.raw_value = raw;
        sample.timestamp_ms = ++sequence * 25U;
        sample.valid = true;
        TestMock_SetTimeMs(sample.timestamp_ms);
        if (!MetrologyManager_AcceptRawSample(&sample)) return 4;
        MetrologyManager_Process20ms();
        weight = MetrologyManager_GetSnapshot();
        r5 = MetrologyManager_GetA13CSnapshot();
        if (weight == NULL || r5 == NULL || FaultManager_GetActiveMask() ||
                TestMock_GetSaveRequestCount() || r5->limited) return 5;
        if (sequence > 100U && (MetrologyManager_GetR5Application() != R5_BETA_APPLICATION_ACTIVE ||
                r5->sample_sequence != weight->sample_sequence ||
                weight->net_mass_ug != weight->uncompensated_gross_mass_ug - r5->offset_ug)) return 6;
        printf("%" PRIu32 ",%" PRId64 ",%" PRId64 ",%" PRId64 ",%u,%u,%" PRId64
            ",%" PRIu32 ",%" PRIu32 ",%u,%u\n", sequence,
            weight->uncompensated_gross_mass_ug, weight->net_mass_ug, r5->offset_ug,
            (unsigned)r5->state, (unsigned)r5->reason, r5->reference_twice_ug,
            r5->reference_lock_sequence, r5->first_correction_sequence,
            r5->limited ? 1U : 0U, (unsigned)MetrologyManager_GetR5Application());
    }
    return ferror(stdin) ? 7 : 0;
}
