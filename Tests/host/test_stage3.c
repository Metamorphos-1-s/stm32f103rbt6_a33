#include "calibration_model.h"
#include "cs1237.h"
#include "default_config.h"
#include "fault_manager.h"
#include "measurement_bridge.h"
#include "metrology_config_validator.h"
#include "metrology_manager.h"
#include "mock_hal.h"
#include "project_config.h"
#include "stage3_metrology_diagnostics.h"
#include "stability_detector.h"
#include "synthetic_scale_source.h"
#include "system_context.h"
#include "weight_engine.h"
#include "weight_filter.h"
#include "weight_math.h"
#include "zero_tare.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static unsigned int s_stage3_failures;

#define CHECK3(condition) do { \
    if (!(condition)) { \
        ++s_stage3_failures; \
        (void)printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition); \
    } \
} while (0)

static void Stage3_MakeConfig(DeviceConfig *config, bool calibrated)
{
    CalibrationConfig calibration;

    DefaultConfig_Load(config);
    config->metrology.zero_range = 1000U;
    config->metrology.overload_threshold = 12000U;
    config->stability.window_size = 2U;
    config->stability.enter_threshold = 2U;
    config->stability.exit_threshold = 4U;
    config->stability.stable_hold_ms = 10U;
    if (calibrated)
    {
        CHECK3(CalibrationModel_Build(100000, 1100000, 10000, 1U,
                                      &calibration) == CALIBRATION_RESULT_OK);
        config->calibration = calibration;
    }
}

static bool Stage3_Process(WeightEngine *engine, int32_t raw,
                           uint32_t timestamp_ms)
{
    RawMeasurementSample sample = {raw, timestamp_ms, true};

    return WeightEngine_ProcessRawSample(engine, &sample);
}

static void TestWeightMath(void)
{
    int64_t result64;
    int32_t result32;
    uint32_t absolute;

    CHECK3(WeightMath_DivideRoundNearest(7, 3, &result64) && result64 == 2);
    CHECK3(WeightMath_DivideRoundNearest(8, 3, &result64) && result64 == 3);
    CHECK3(WeightMath_DivideRoundNearest(-7, 3, &result64) && result64 == -2);
    CHECK3(WeightMath_DivideRoundNearest(-8, 3, &result64) && result64 == -3);
    CHECK3(WeightMath_DivideRoundNearest(5, 2, &result64) && result64 == 3);
    CHECK3(WeightMath_DivideRoundNearest(-5, 2, &result64) && result64 == -3);
    CHECK3(!WeightMath_DivideRoundNearest(1, 0, &result64));
    CHECK3(!WeightMath_DivideRoundNearest(INT64_MIN, -1, &result64));
    CHECK3(WeightMath_AbsInt32(INT32_MIN, &absolute) &&
           absolute == 2147483648UL);
    CHECK3(WeightMath_ClampInt64ToInt32(INT32_MAX, &result32));
    CHECK3(!WeightMath_ClampInt64ToInt32((int64_t)INT32_MAX + 1, &result32));
    CHECK3(WeightMath_Quantize(13, 1U, &result32) && result32 == 13);
    CHECK3(WeightMath_Quantize(3, 2U, &result32) && result32 == 4);
    CHECK3(WeightMath_Quantize(-3, 2U, &result32) && result32 == -4);
    CHECK3(WeightMath_Quantize(12, 5U, &result32) && result32 == 10);
    CHECK3(WeightMath_Quantize(13, 5U, &result32) && result32 == 15);
    CHECK3(WeightMath_Quantize(-12, 5U, &result32) && result32 == -10);
    CHECK3(WeightMath_Quantize(-13, 5U, &result32) && result32 == -15);
    CHECK3(!WeightMath_Quantize(1, 0U, &result32));
}

static void TestWeightFilter(void)
{
    WeightFilter filter;
    int32_t output = 0;
    uint8_t index;

    CHECK3(WeightFilter_Init(&filter, FILTER_MODE_NONE, 255U));
    CHECK3(WeightFilter_Process(&filter, -123, &output) && output == -123);
    CHECK3(WeightFilter_IsReady(&filter));

    CHECK3(WeightFilter_Init(&filter, FILTER_MODE_AVERAGE, 4U));
    CHECK3(WeightFilter_Process(&filter, 1, &output) && output == 1);
    CHECK3(!WeightFilter_IsReady(&filter));
    CHECK3(WeightFilter_Process(&filter, 2, &output) && output == 2);
    CHECK3(WeightFilter_Process(&filter, 3, &output) && output == 2);
    CHECK3(WeightFilter_Process(&filter, 4, &output) && output == 3);
    CHECK3(WeightFilter_IsReady(&filter));
    CHECK3(WeightFilter_Process(&filter, 5, &output) && output == 4);
    CHECK3(WeightFilter_GetAcceptedCount(&filter) == 5U);

    CHECK3(WeightFilter_Init(&filter, FILTER_MODE_AVERAGE, 8U));
    for (index = 1U; index <= 8U; ++index)
    {
        CHECK3(WeightFilter_Process(&filter, index, &output));
    }
    CHECK3(output == 5 && WeightFilter_IsReady(&filter));
    CHECK3(WeightFilter_Init(&filter, FILTER_MODE_AVERAGE, 2U));
    CHECK3(WeightFilter_Process(&filter, -1, &output));
    CHECK3(WeightFilter_Process(&filter, -2, &output) && output == -2);

    CHECK3(WeightFilter_Init(&filter, FILTER_MODE_IIR, 1U));
    CHECK3(WeightFilter_Process(&filter, 0, &output) && output == 0);
    CHECK3(WeightFilter_Process(&filter, 8, &output) && output == 4);
    CHECK3(WeightFilter_Process(&filter, 8, &output) && output == 6);
    CHECK3(WeightFilter_Process(&filter, -8, &output) && output == -1);

    CHECK3(WeightFilter_Init(&filter, FILTER_MODE_MEDIAN3_IIR, 1U));
    CHECK3(WeightFilter_Process(&filter, 0, &output) && output == 0);
    CHECK3(WeightFilter_Process(&filter, 1000, &output) && output == 0);
    CHECK3(WeightFilter_Process(&filter, 0, &output) && output == 0);
    CHECK3(WeightFilter_IsReady(&filter));
    WeightFilter_Reset(&filter);
    CHECK3(!WeightFilter_IsReady(&filter));
    CHECK3(WeightFilter_GetAcceptedCount(&filter) == 0U);
    CHECK3(!WeightFilter_Init(&filter, FILTER_MODE_AVERAGE, 1U));
    CHECK3(!WeightFilter_Init(&filter, FILTER_MODE_IIR, 0U));
    CHECK3(!WeightFilter_Init(&filter, FILTER_MODE_IIR, 9U));
}

static void TestCalibration(void)
{
    CalibrationConfig calibration;
    WeightValue weight;

    CHECK3(CalibrationModel_Build(100000, 1100000, 10000, 7U,
                                  &calibration) == CALIBRATION_RESULT_OK);
    CHECK3(calibration.calibration_valid);
    CHECK3(calibration.calibration_sequence == 7U);
    CHECK3(CalibrationModel_Validate(&calibration) == CALIBRATION_RESULT_OK);
    CHECK3(CalibrationModel_Convert(&calibration, 100000, 0, &weight) ==
           CALIBRATION_RESULT_OK && weight == 0);
    CHECK3(CalibrationModel_Convert(&calibration, 600000, 0, &weight) ==
           CALIBRATION_RESULT_OK && weight == 5000);
    CHECK3(CalibrationModel_Convert(&calibration, 1100000, 0, &weight) ==
           CALIBRATION_RESULT_OK && weight == 10000);
    CHECK3(CalibrationModel_Convert(&calibration, 200000, 100000, &weight) ==
           CALIBRATION_RESULT_OK && weight == 0);

    CHECK3(CalibrationModel_Build(1100000, 100000, 10000, 8U,
                                  &calibration) == CALIBRATION_RESULT_OK);
    CHECK3(calibration.scale_denominator < 0);
    CHECK3(CalibrationModel_Convert(&calibration, 600000, 0, &weight) ==
           CALIBRATION_RESULT_OK && weight == 5000);
    CHECK3(CalibrationModel_Build(1, 1, 100, 0U, &calibration) ==
           CALIBRATION_RESULT_INVALID_SPAN);
    CHECK3(CalibrationModel_Build(1, 1001, 100, 0U, &calibration) ==
           CALIBRATION_RESULT_SPAN_TOO_SMALL);
    CHECK3(CalibrationModel_Build(1, 2000, 0, 0U, &calibration) ==
           CALIBRATION_RESULT_INVALID_WEIGHT);

    CHECK3(CalibrationModel_Build(100000, 1100000, 10000, 1U,
                                  &calibration) == CALIBRATION_RESULT_OK);
    ++calibration.scale_denominator;
    CHECK3(CalibrationModel_Validate(&calibration) ==
           CALIBRATION_RESULT_INCONSISTENT);
    CHECK3(CalibrationModel_Build(-2000000000, -1999998000, INT32_MAX, 1U,
                                  &calibration) == CALIBRATION_RESULT_OK);
    CHECK3(CalibrationModel_Convert(&calibration, 2000000000, 0, &weight) ==
           CALIBRATION_RESULT_OVERFLOW);
}

static void TestStability(void)
{
    StabilityConfig config = {2U, 2U, 4U, 10U};
    StabilityDetector detector;

    CHECK3(StabilityDetector_Init(&detector, &config));
    CHECK3(StabilityDetector_Process(&detector, -10, 0xFFFFFFF0U) ==
           STABILITY_STATE_UNAVAILABLE);
    CHECK3(StabilityDetector_Process(&detector, -10, 0xFFFFFFF5U) ==
           STABILITY_STATE_CANDIDATE);
    CHECK3(StabilityDetector_Process(&detector, -10, 0xFFFFFFFEU) ==
           STABILITY_STATE_CANDIDATE);
    CHECK3(StabilityDetector_Process(&detector, -10, 0x00000000U) ==
           STABILITY_STATE_STABLE);
    CHECK3(StabilityDetector_Process(&detector, -7, 0x00000001U) ==
           STABILITY_STATE_STABLE);
    CHECK3(StabilityDetector_GetSpread(&detector) == 3U);
    CHECK3(StabilityDetector_Process(&detector, -12, 0x00000002U) ==
           STABILITY_STATE_UNSTABLE);
    StabilityDetector_Reset(&detector);
    CHECK3(StabilityDetector_GetState(&detector) ==
           STABILITY_STATE_UNAVAILABLE);
    CHECK3(StabilityDetector_Process(&detector, 1, 100U) ==
           STABILITY_STATE_UNAVAILABLE);

    config.window_size = 1U;
    CHECK3(!StabilityDetector_Init(&detector, &config));
}

static void TestZeroTare(void)
{
    ZeroTareState state;

    ZeroTare_Init(&state, 0, false);
    CHECK3(ZeroTare_ApplyZero(&state, 100100, 100000, 1, 10U, true,
                              false) == WEIGHT_ACTION_CALIBRATION_INVALID);
    CHECK3(ZeroTare_ApplyZero(&state, 100100, 100000, 1, 10U, false,
                              true) == WEIGHT_ACTION_NOT_STABLE);
    CHECK3(ZeroTare_ApplyZero(&state, 200000, 100000, 11, 10U, true,
                              true) == WEIGHT_ACTION_OUT_OF_ZERO_RANGE);
    CHECK3(ZeroTare_ApplyZero(&state, 100100, 100000, 1, 10U, true,
                              true) == WEIGHT_ACTION_OK);
    CHECK3(state.zero_offset_raw == 100);
    CHECK3(ZeroTare_ResetZero(&state) == WEIGHT_ACTION_OK &&
           state.zero_offset_raw == 0);
    CHECK3(ZeroTare_ApplyTare(&state, 5000, false, true, false) ==
           WEIGHT_ACTION_NOT_STABLE);
    CHECK3(ZeroTare_ApplyTare(&state, 5000, true, true, true) ==
           WEIGHT_ACTION_OVERLOAD);
    CHECK3(ZeroTare_ApplyTare(&state, 5000, true, true, false) ==
           WEIGHT_ACTION_OK && state.tare_weight == 5000);
    CHECK3(ZeroTare_ApplyZero(&state, 100000, 100000, 0, 10U, true,
                              true) == WEIGHT_ACTION_TARE_ACTIVE);
    CHECK3(ZeroTare_ApplyTare(&state, 6000, true, true, false) ==
           WEIGHT_ACTION_OK && state.tare_weight == 6000);
    CHECK3(ZeroTare_ClearTare(&state) == WEIGHT_ACTION_OK &&
           !state.tare_active && state.tare_weight == 0);
}

static void TestMetrologyConfig(void)
{
    DeviceConfig config;

    Stage3_MakeConfig(&config, false);
    CHECK3(MetrologyConfig_Validate(&config.metrology, &config.stability) ==
           METROLOGY_CONFIG_OK);
    config.metrology.capacity = 0U;
    CHECK3(MetrologyConfig_Validate(&config.metrology, &config.stability) ==
           METROLOGY_CONFIG_INVALID_CAPACITY);
    Stage3_MakeConfig(&config, false);
    config.metrology.division = 0U;
    CHECK3(MetrologyConfig_Validate(&config.metrology, &config.stability) ==
           METROLOGY_CONFIG_INVALID_DIVISION);
    Stage3_MakeConfig(&config, false);
    config.metrology.filter_mode = FILTER_MODE_AVERAGE;
    config.metrology.filter_strength = 1U;
    CHECK3(MetrologyConfig_Validate(&config.metrology, &config.stability) ==
           METROLOGY_CONFIG_INVALID_FILTER);
}

static void TestManagerInitializationFaults(void)
{
    DeviceConfig config;

    Stage3_MakeConfig(&config, false);
    CHECK3(SystemContext_Init(&config, 0U));
    FaultManager_Init();
    config.metrology.division = 0U;
    CHECK3(!MetrologyManager_Init(&config, &SystemContext_Get()->runtime));
    CHECK3(!MetrologyManager_IsInitialized());
    CHECK3(FaultManager_IsActive(FAULT_METROLOGY_CONFIG_INVALID));

    Stage3_MakeConfig(&config, true);
    CHECK3(SystemContext_Init(&config, 0U));
    FaultManager_Init();
    ++config.calibration.scale_denominator;
    CHECK3(!MetrologyManager_Init(&config, &SystemContext_Get()->runtime));
    CHECK3(!MetrologyManager_IsInitialized());
    CHECK3(FaultManager_IsActive(FAULT_CALIBRATION_DATA_CORRUPT));
}

static void TestWeightEngine(void)
{
    DeviceConfig config;
    WeightEngine engine;
    const WeightSnapshot *snapshot;
    CalibrationConfig calibration;

    Stage3_MakeConfig(&config, false);
    CHECK3(WeightEngine_Init(&engine, &config.metrology, &config.calibration,
                             &config.stability, 0, false));
    CHECK3(Stage3_Process(&engine, 123456, 1U));
    snapshot = WeightEngine_GetSnapshot(&engine);
    CHECK3(snapshot->raw_value == 123456);
    CHECK3(snapshot->filtered_raw == 123456);
    CHECK3((snapshot->status_flags & WEIGHT_STATUS_RAW_VALID) != 0U);
    CHECK3((snapshot->status_flags & WEIGHT_STATUS_FILTER_READY) != 0U);
    CHECK3((snapshot->status_flags & WEIGHT_STATUS_WEIGHT_VALID) == 0U);
    CHECK3(WeightEngine_Zero(&engine) ==
           WEIGHT_ACTION_CALIBRATION_INVALID);
    CHECK3(WeightEngine_Tare(&engine) ==
           WEIGHT_ACTION_CALIBRATION_INVALID);

    Stage3_MakeConfig(&config, true);
    config.metrology.division = 5U;
    CHECK3(WeightEngine_Init(&engine, &config.metrology, &config.calibration,
                             &config.stability, 0, false));
    CHECK3(Stage3_Process(&engine, 101300, 0U));
    snapshot = WeightEngine_GetSnapshot(&engine);
    CHECK3(snapshot->gross_unrounded == 13);
    CHECK3(snapshot->gross_weight == 15);
    CHECK3(snapshot->sample_sequence == 1U);

    CHECK3(Stage3_Process(&engine, 105000, 10U));
    CHECK3(Stage3_Process(&engine, 105000, 20U));
    CHECK3(Stage3_Process(&engine, 105000, 30U));
    CHECK3((WeightEngine_GetSnapshot(&engine)->status_flags &
            WEIGHT_STATUS_STABLE) != 0U);
    CHECK3(WeightEngine_Zero(&engine) == WEIGHT_ACTION_OK);
    CHECK3(WeightEngine_GetSnapshot(&engine)->gross_unrounded == 0);
    CHECK3((WeightEngine_GetSnapshot(&engine)->status_flags &
            WEIGHT_STATUS_STABLE) == 0U);
    CHECK3(WeightEngine_ResetZero(&engine) == WEIGHT_ACTION_OK);
    CHECK3(WeightEngine_GetSnapshot(&engine)->gross_unrounded == 50);

    Stage3_MakeConfig(&config, true);
    CHECK3(WeightEngine_Init(&engine, &config.metrology, &config.calibration,
                             &config.stability, 0, false));
    CHECK3(Stage3_Process(&engine, 600000, 0U));
    CHECK3(Stage3_Process(&engine, 600000, 10U));
    CHECK3(Stage3_Process(&engine, 600000, 20U));
    CHECK3(WeightEngine_Tare(&engine) == WEIGHT_ACTION_OK);
    CHECK3(WeightEngine_GetSnapshot(&engine)->tare_weight == 5000);
    CHECK3(WeightEngine_GetSnapshot(&engine)->net_unrounded == 0);
    CHECK3(Stage3_Process(&engine, 700000, 30U));
    CHECK3(Stage3_Process(&engine, 700000, 40U));
    CHECK3(Stage3_Process(&engine, 700000, 50U));
    CHECK3(WeightEngine_Tare(&engine) == WEIGHT_ACTION_OK);
    CHECK3(WeightEngine_GetSnapshot(&engine)->tare_weight == 6000);
    CHECK3(WeightEngine_ClearTare(&engine) == WEIGHT_ACTION_OK);
    CHECK3(WeightEngine_GetSnapshot(&engine)->net_unrounded == 6000);

    CHECK3(Stage3_Process(&engine, 1310000, 60U));
    CHECK3((WeightEngine_GetSnapshot(&engine)->status_flags &
            WEIGHT_STATUS_OVERLOAD) != 0U);
    CHECK3(Stage3_Process(&engine, -1110000, 70U));
    CHECK3((WeightEngine_GetSnapshot(&engine)->status_flags &
            WEIGHT_STATUS_OVERLOAD) != 0U);

    CHECK3(CalibrationModel_Build(100000, 1100000, 10000, 2U,
                                  &calibration) == CALIBRATION_RESULT_OK);
    CHECK3(WeightEngine_ApplyCalibration(&engine, &calibration));
    CHECK3(StabilityDetector_GetState(&engine.stability) ==
           STABILITY_STATE_UNAVAILABLE);
    CHECK3(WeightEngine_ReconfigureFilter(&engine, FILTER_MODE_AVERAGE, 4U));
    CHECK3(WeightEngine_GetSnapshot(&engine)->filter_sample_count == 0U);
    CHECK3((WeightEngine_GetSnapshot(&engine)->status_flags &
            WEIGHT_STATUS_FILTER_READY) == 0U);
}

static void TestManagerEvents(void)
{
    DeviceConfig config;
    RawMeasurementSample sample = {600000, 0U, true};

    Stage3_MakeConfig(&config, true);
    CHECK3(SystemContext_Init(&config, 0U));
    TestMock_Reset();
    CHECK3(MetrologyManager_Init(&config, &SystemContext_Get()->runtime));
    CHECK3(MetrologyManager_AcceptRawSample(&sample));
    TestMock_RejectNextEvents(1U);
    MetrologyManager_Process20ms();
    CHECK3(TestMock_GetEventTypeCount(EVENT_NEW_WEIGHT_SAMPLE) == 0U);
    MetrologyManager_Process20ms();
    CHECK3(TestMock_GetEventTypeCount(EVENT_NEW_WEIGHT_SAMPLE) == 1U);
    MetrologyManager_Process20ms();
    CHECK3(TestMock_GetEventTypeCount(EVENT_NEW_WEIGHT_SAMPLE) == 1U);

    sample.timestamp_ms = 10U;
    CHECK3(MetrologyManager_AcceptRawSample(&sample));
    sample.timestamp_ms = 20U;
    CHECK3(MetrologyManager_AcceptRawSample(&sample));
    TestMock_RejectEventTypeOnce(EVENT_WEIGHT_STABLE_CHANGED);
    MetrologyManager_Process20ms();
    CHECK3(TestMock_GetEventTypeCount(EVENT_WEIGHT_STABLE_CHANGED) == 0U);
    MetrologyManager_Process20ms();
    CHECK3(TestMock_GetEventTypeCount(EVENT_WEIGHT_STABLE_CHANGED) == 1U);
    sample.raw_value = 700000;
    sample.timestamp_ms = 30U;
    CHECK3(MetrologyManager_AcceptRawSample(&sample));
    MetrologyManager_Process20ms();
    CHECK3(TestMock_GetEventTypeCount(EVENT_WEIGHT_STABLE_CHANGED) == 2U);

    CHECK3(MetrologyManager_Tare() == WEIGHT_ACTION_NOT_STABLE);
    Stage3MetrologyDiagnostics_Update();
    CHECK3(Stage3MetrologyDiagnostics_GetSnapshot()->sample_sequence == 4U);
}

static void TestBridgeIntegration(void)
{
    DeviceConfig config;
    CS1237_Config cs_config = {
        CS1237_RATE_10_HZ, CS1237_GAIN_128, CS1237_CHANNEL_A, true
    };
    CS1237_Sample sample = {0};
    uint16_t index;

    Stage3_MakeConfig(&config, true);
    CHECK3(SystemContext_Init(&config, 0U));
    CHECK3(MetrologyManager_Init(&config, &SystemContext_Get()->runtime));
    TestMock_Reset();
    CHECK3(CS1237_Init(&cs_config));
    MeasurementBridge_Init();
    for (index = 0U; index < CS1237_SAMPLE_BUFFER_CAPACITY; ++index)
    {
        sample.raw = 100000 + (int32_t)index * 100;
        sample.timestamp_ms = index;
        sample.valid = true;
        CHECK3(CS1237_TestPushSample(&sample));
    }
    for (index = 0U; index < 4U; ++index)
    {
        CHECK3(MeasurementBridge_Process(4U) == 4U);
        CHECK3(MetrologyManager_GetSnapshot()->sample_sequence ==
               (uint32_t)(index + 1U) * 4U);
    }
    CHECK3(MeasurementBridge_GetLastBacklog() == 0U);
    CHECK3(CS1237_GetBufferOverrunCount() == 0U);
    MetrologyManager_Process20ms();
    CHECK3(TestMock_GetEventTypeCount(EVENT_NEW_WEIGHT_SAMPLE) == 1U);
}

static void TestSyntheticScaleFlow(void)
{
    DeviceConfig config;
    WeightEngine engine;
    SyntheticScaleSource source;
    RawMeasurementSample sample;
    uint8_t index;

    Stage3_MakeConfig(&config, true);
    CHECK3(WeightEngine_Init(&engine, &config.metrology, &config.calibration,
                             &config.stability, 0, false));
    CHECK3(SyntheticScaleSource_Init(&source, 100000, 100,
                                     0xFFFFFFF0U, 10U));
    SyntheticScaleSource_SetConstant(&source, 0, 1);
    for (index = 0U; index < 4U; ++index)
    {
        CHECK3(SyntheticScaleSource_Next(&source, &sample));
        CHECK3(WeightEngine_ProcessRawSample(&engine, &sample));
    }
    SyntheticScaleSource_SetVibration(&source, 5000, 100);
    for (index = 0U; index < 2U; ++index)
    {
        CHECK3(SyntheticScaleSource_Next(&source, &sample));
        CHECK3(WeightEngine_ProcessRawSample(&engine, &sample));
    }
    SyntheticScaleSource_SetConstant(&source, 5000, 0);
    for (index = 0U; index < 3U; ++index)
    {
        CHECK3(SyntheticScaleSource_Next(&source, &sample));
        CHECK3(WeightEngine_ProcessRawSample(&engine, &sample));
    }
    CHECK3(WeightEngine_Tare(&engine) == WEIGHT_ACTION_OK);
    CHECK3(WeightEngine_GetSnapshot(&engine)->net_weight == 0);
    SyntheticScaleSource_SetConstant(&source, 6000, 0);
    for (index = 0U; index < 3U; ++index)
    {
        CHECK3(SyntheticScaleSource_Next(&source, &sample));
        CHECK3(WeightEngine_ProcessRawSample(&engine, &sample));
    }
    CHECK3(WeightEngine_GetSnapshot(&engine)->net_weight == 1000);
    CHECK3(WeightEngine_ClearTare(&engine) == WEIGHT_ACTION_OK);
    CHECK3(WeightEngine_GetSnapshot(&engine)->net_weight == 6000);

    SyntheticScaleSource_SetSingleSpike(&source, 0, 1U, 50000);
    CHECK3(SyntheticScaleSource_Next(&source, &sample));
    CHECK3(SyntheticScaleSource_Next(&source, &sample) &&
           sample.raw_value == 150000);
    SyntheticScaleSource_SetVibration(&source, 0, 10);
    CHECK3(SyntheticScaleSource_Next(&source, &sample) &&
           sample.raw_value == 99990);
    CHECK3(SyntheticScaleSource_Next(&source, &sample) &&
           sample.raw_value == 100010);
    SyntheticScaleSource_SetRamp(&source, 0, 1);
    CHECK3(SyntheticScaleSource_Next(&source, &sample) &&
           sample.raw_value == 100000);
    CHECK3(SyntheticScaleSource_Next(&source, &sample) &&
           sample.raw_value == 100100);

    CHECK3(SyntheticScaleSource_Init(&source, 100000, -100, 0U, 10U));
    SyntheticScaleSource_SetConstant(&source, 5000, 0);
    CHECK3(SyntheticScaleSource_Next(&source, &sample) &&
           sample.raw_value == -400000);
}

static void TestInternalReferenceRegression(void)
{
    volatile uint32_t reference_output_enabled =
        CS1237_REFERENCE_OUTPUT_ENABLED;
    CS1237_Config config = {
        CS1237_RATE_10_HZ, CS1237_GAIN_128, CS1237_CHANNEL_A,
        (CS1237_REFERENCE_OUTPUT_ENABLED != 0U)
    };
    uint8_t encoded = 0U;

    CHECK3(reference_output_enabled == 1U);
    CHECK3(CS1237_EncodeConfig(&config, &encoded));
    CHECK3(encoded == 0x0CU);
}

unsigned int Stage3_RunTests(void)
{
    TestWeightMath();
    TestWeightFilter();
    TestCalibration();
    TestStability();
    TestZeroTare();
    TestMetrologyConfig();
    TestManagerInitializationFaults();
    TestWeightEngine();
    TestManagerEvents();
    TestBridgeIntegration();
    TestSyntheticScaleFlow();
    TestInternalReferenceRegression();
    return s_stage3_failures;
}
