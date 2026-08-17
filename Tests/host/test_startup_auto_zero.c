#include "startup_auto_zero_controller.h"

#include <assert.h>
#include <stdio.h>

static StartupAutoZeroInput Ready(uint32_t now_ms, MassValueUg gross)
{
    StartupAutoZeroInput input = {0};
    input.now_ms = now_ms;
    input.app_running = true;
    input.measurement_ready = true;
    input.stable = true;
    input.calibration_valid = true;
    input.gross_mass_ug = gross;
    input.zero_range_ug = INT64_C(60000000);
    return input;
}

static void TestDisabledAndTare(void)
{
    StartupAutoZeroController controller;
    assert(StartupAutoZeroController_Init(&controller, false, false, 10000U));
    assert(controller.snapshot.state == STARTUP_AUTO_ZERO_DISABLED);
    assert(controller.snapshot.terminal);
    assert(!StartupAutoZeroController_Init(NULL, true, false, 10000U));
    assert(!StartupAutoZeroController_Init(&controller, true, false, 0U));
    assert(StartupAutoZeroController_Init(&controller, true, true, 10000U));
    assert(controller.snapshot.state == STARTUP_AUTO_ZERO_SKIPPED_TARE);
}

static void TestAppliedOnce(void)
{
    StartupAutoZeroController controller;
    StartupAutoZeroInput input = Ready(100U, INT64_C(1000000));
    assert(StartupAutoZeroController_Init(&controller, true, false, 10000U));
    input.app_running = false;
    assert(!StartupAutoZeroController_Process(&controller, &input));
    input.app_running = true;
    assert(StartupAutoZeroController_Process(&controller, &input));
    assert(!StartupAutoZeroController_Process(&controller, &input));
    StartupAutoZeroController_CompleteZero(&controller, WEIGHT_ACTION_OK);
    assert(controller.snapshot.state == STARTUP_AUTO_ZERO_APPLIED);
    assert(controller.snapshot.terminal);
    assert(!StartupAutoZeroController_Process(&controller, &input));
}

static void TestWaitTimeoutAndRange(void)
{
    StartupAutoZeroController controller;
    StartupAutoZeroInput input = Ready(20U, 0);
    assert(StartupAutoZeroController_Init(&controller, true, false, 10000U));
    input.measurement_ready = false;
    assert(!StartupAutoZeroController_Process(&controller, &input));
    assert(controller.snapshot.state == STARTUP_AUTO_ZERO_WAIT_MEASUREMENT);
    input.measurement_ready = true;
    input.stable = false;
    input.now_ms = 100U;
    assert(!StartupAutoZeroController_Process(&controller, &input));
    assert(controller.snapshot.state == STARTUP_AUTO_ZERO_WAIT_STABLE);
    input.now_ms = 10020U;
    assert(!StartupAutoZeroController_Process(&controller, &input));
    assert(controller.snapshot.state == STARTUP_AUTO_ZERO_TIMEOUT);

    assert(StartupAutoZeroController_Init(&controller, true, false, 10000U));
    input = Ready(1U, INT64_C(60000001));
    assert(!StartupAutoZeroController_Process(&controller, &input));
    assert(controller.snapshot.state == STARTUP_AUTO_ZERO_SKIPPED_RANGE);
    input.gross_mass_ug = 0;
    assert(!StartupAutoZeroController_Process(&controller, &input));
}

static void TestInvalidAndFault(void)
{
    StartupAutoZeroController controller;
    StartupAutoZeroInput input = Ready(0U, 0);
    assert(StartupAutoZeroController_Init(&controller, true, false, 10000U));
    input.calibration_valid = false;
    assert(!StartupAutoZeroController_Process(&controller, &input));
    assert(controller.snapshot.state == STARTUP_AUTO_ZERO_INVALID_CALIBRATION);
    assert(StartupAutoZeroController_Init(&controller, true, false, 10000U));
    input.calibration_valid = true;
    input.weight_fault = true;
    assert(!StartupAutoZeroController_Process(&controller, &input));
    assert(controller.snapshot.state == STARTUP_AUTO_ZERO_FAULT);
}

int main(void)
{
    TestDisabledAndTare();
    TestAppliedOnce();
    TestWaitTimeoutAndRange();
    TestInvalidAndFault();
    puts("Startup auto-zero tests passed");
    return 0;
}
