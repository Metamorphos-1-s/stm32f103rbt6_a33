#include "calibration_model.h"
#include "default_config.h"
#include "weight_engine.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

int main(void)
{
    DeviceConfig config;
    CalibrationConfig calibration;
    WeightEngine engine;
    RawMeasurementSample sample = {0};
    const WeightSnapshot *snapshot;
    DefaultConfig_Load(&config);
    CHECK(CalibrationModel_BuildMass(0, -500000, 500000000, 1U,
                                     &calibration) == CALIBRATION_RESULT_OK);
    config.calibration = calibration;
    config.metrology.profiles[0].filter_mode = FILTER_MODE_NONE;
    config.metrology.profiles[0].filter_strength = 0U;
    CHECK(WeightEngine_InitMass(&engine, &config.metrology,
        &config.calibration, &config.stability, 0, false));
    sample.raw_value = -500000;
    sample.timestamp_ms = 100U;
    sample.valid = true;
    CHECK(WeightEngine_ProcessRawSample(&engine, &sample));
    snapshot = WeightEngine_GetSnapshot(&engine);
    CHECK(snapshot != NULL);
    CHECK(snapshot->uncompensated_gross_mass_ug == 500000000);
    CHECK(snapshot->gross_mass_ug == 500000000);
    CHECK(WeightEngine_SetBetaExternalDrift(&engine, 200000, false));
    snapshot = WeightEngine_GetSnapshot(&engine);
    CHECK(snapshot->gross_mass_ug == 500000000);
    CHECK(WeightEngine_SetBetaExternalDrift(&engine, 200000, true));
    snapshot = WeightEngine_GetSnapshot(&engine);
    CHECK(snapshot->gross_mass_ug == 499800000);
    CHECK(snapshot->net_mass_ug == 499800000);
    CHECK(!WeightEngine_SetRuntimeDriftEnabled(&engine, true));
    CHECK(WeightEngine_SetBetaExternalDrift(&engine, 0, false));
    CHECK(WeightEngine_GetSnapshot(&engine)->gross_mass_ug == 500000000);
    return 0;
}
