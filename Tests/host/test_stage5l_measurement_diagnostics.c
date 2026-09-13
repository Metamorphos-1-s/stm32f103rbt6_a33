#include "stage5l_measurement_diagnostics.h"
#include "metrology_manager.h"
#include "system_context.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static SystemContext s_context;
static bool s_reconfigure_result = true;
static FilterMode s_last_mode;
static uint8_t s_last_strength;

const SystemContext *SystemContext_Get(void) { return &s_context; }
bool SystemContext_SetConfigDirty(bool dirty)
{
    s_context.runtime.config_dirty = dirty;
    return true;
}
bool MetrologyManager_ReconfigureFilter(FilterMode mode, uint8_t strength)
{
    s_last_mode = mode;
    s_last_strength = strength;
    s_context.runtime.config_dirty = true;
    return s_reconfigure_result;
}

#define CHECK(x) do { if (!(x)) { printf("FAIL:%d\n", __LINE__); return 1; } } while (0)

int main(void)
{
    (void)memset(&s_context, 0, sizeof(s_context));
    s_context.config.metrology.active_profile =
        WEIGHING_PROFILE_HIGH_PRECISION;
    s_context.config.metrology.profiles[0].filter_mode =
        FILTER_MODE_MEDIAN3_IIR;
    s_context.config.metrology.profiles[0].filter_strength = 3U;
    Stage5LMeasurementDiagnostics_Init();
    CHECK(g_stage5l_measurement_control.magic == STAGE5L_DIAGNOSTIC_MAGIC);
    CHECK(g_stage5l_measurement_control.effective_mode ==
          FILTER_MODE_MEDIAN3_IIR);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_FILTER;
    g_stage5l_measurement_control.requested_mode = FILTER_MODE_IIR;
    g_stage5l_measurement_control.requested_strength = 1U;
    g_stage5l_measurement_control.request_sequence = 1U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(s_last_mode == FILTER_MODE_IIR && s_last_strength == 1U);
    CHECK(!s_context.runtime.config_dirty);
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_APPLIED);
    CHECK(g_stage5l_measurement_control.override_active == 1U);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_RESTORE_FILTER;
    g_stage5l_measurement_control.request_sequence = 2U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(s_last_mode == FILTER_MODE_MEDIAN3_IIR && s_last_strength == 3U);
    CHECK(!s_context.runtime.config_dirty);
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_RESTORED);
    CHECK(g_stage5l_measurement_control.override_active == 0U);

    g_stage5l_measurement_control.command =
        STAGE5L_DIAGNOSTIC_COMMAND_APPLY_FILTER;
    g_stage5l_measurement_control.requested_mode = FILTER_MODE_COUNT;
    g_stage5l_measurement_control.request_sequence = 3U;
    Stage5LMeasurementDiagnostics_Process();
    CHECK(g_stage5l_measurement_control.status ==
          STAGE5L_DIAGNOSTIC_STATUS_INVALID);
    CHECK(!s_context.runtime.config_dirty);
    puts("stage5l diagnostics tests passed");
    return 0;
}
