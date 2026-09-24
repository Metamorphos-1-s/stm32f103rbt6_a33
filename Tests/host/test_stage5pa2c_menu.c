#include "bsp_time.h"
#include "checkweigh_local_control.h"
#include "command_service.h"
#include "config_application.h"
#include "config_edit.h"
#include "display_codes.h"
#include "display_controller.h"
#include "mass_math.h"
#include "menu_controller.h"
#include "metrology_config_validator.h"
#include "persistence_manager.h"
#include "persistent_codec.h"
#include "revision_helper.h"
#include "system_context.h"
#include "ui_config_workspace.h"
#include "unit_converter.h"

#include <stdio.h>
#include <string.h>

static SystemContext context;
static DeviceConfig working;
static uint32_t now_ms;
static unsigned save_requests;
static unsigned runtime_set_requests;
static CommandResult save_result;
static PersistenceStatus save_status;
static bool save_busy;
static bool invalid_config;
static uint32_t checkweigh_generation;
static char last_message[7];
static char last_page[7];

static bool IsDone(void)
{
    char text[6];
    return DisplayCodes_Get(DISPLAY_CODE_DONE, text) &&
        (memcmp(last_message, text, 6U) == 0);
}

#define CHECK(value) do { if (!(value)) { \
    (void)printf("FAIL %d: %s\n", __LINE__, #value); return 1; \
} } while (0)

bsp_time_ms_t BSP_TimeNowMs(void) { return now_ms; }
const SystemContext *SystemContext_Get(void) { return &context; }
uint32_t SystemContext_GetConfigRevision(void)
{ return context.config_revision; }
uint32_t SystemContext_GetSavedRevision(void)
{ return context.saved_revision; }
uint32_t Revision_Next(uint32_t revision) { return revision + 1U; }

CommandResult CommandService_Execute(const CommandRequest *request,
    CommandResponse *response)
{
    (void)memset(response, 0, sizeof(*response));
    if (request->id == COMMAND_R5_GET_STATUS)
    {
        response->value0 = (int32_t)(
            ((uint32_t)context.config.system.requested_r5_application << 24U) |
            ((uint32_t)context.config.system.requested_r5_mode << 16U));
        return COMMAND_RESULT_OK;
    }
    if (request->id == COMMAND_CHECKWEIGH_GET_STATUS)
    {
        response->value0 = context.config.system.requested_checkweigh_mode;
        response->value1 = (int32_t)checkweigh_generation;
        return COMMAND_RESULT_OK;
    }
    ++runtime_set_requests;
    return COMMAND_RESULT_INVALID_STATE;
}

bool ConfigEdit_Begin(const DeviceConfig *current)
{ working = *current; return true; }
void ConfigEdit_Cancel(void) {}
bool ConfigEdit_CopyWorking(DeviceConfig *target)
{ *target = working; return true; }
bool ConfigEdit_SetIntegerField(ConfigFieldId field, int32_t value)
{ (void)field; (void)value; return true; }
bool ConfigEdit_SetMassField(ConfigMassFieldId field, MassValueUg value)
{ (void)field; (void)value; return true; }
bool ConfigEdit_SetUnitDisplay(MassUnit unit, const UnitDisplayConfig *display)
{ (void)unit; (void)display; return true; }
bool ConfigEdit_SetProfileField(WeighingProfileId profile,
    ConfigProfileFieldId field, int64_t value)
{
    WeighingProfileConfig *target = &working.metrology.profiles[profile];
    if (field == CONFIG_PROFILE_FIELD_SAMPLE_RATE)
        target->sample_rate = (Cs1237DataRate)value;
    else if (field == CONFIG_PROFILE_FIELD_FILTER_MODE)
        target->filter_mode = (FilterMode)value;
    else if (field == CONFIG_PROFILE_FIELD_FILTER_STRENGTH)
        target->filter_strength = (uint8_t)value;
    return true;
}
ConfigApplyResult ConfigApplication_Validate(const DeviceConfig *candidate,
    bool allow_cs1237_change)
{
    (void)candidate; (void)allow_cs1237_change;
    return invalid_config ? CONFIG_APPLY_INVALID : CONFIG_APPLY_OK;
}
bool PersistentCodec_DeviceConfigEqual(const DeviceConfig *left,
    const DeviceConfig *right)
{ return memcmp(left, right, sizeof(*left)) == 0; }
bool MetrologyConfig_FilterStrengthBounds(FilterMode mode,
    uint8_t *minimum, uint8_t *maximum)
{
    if (mode == FILTER_MODE_NONE) { *minimum = 0U; *maximum = 8U; }
    else if (mode == FILTER_MODE_AVERAGE) {
        *minimum = 2U; *maximum = 32U;
    }
    else if ((mode == FILTER_MODE_IIR) ||
             (mode == FILTER_MODE_MEDIAN3_IIR)) {
        *minimum = 1U; *maximum = 8U;
    }
    else return false;
    return true;
}

bool PersistenceManager_IsBusy(void) { return save_busy; }
PersistenceStatus PersistenceManager_GetStatus(void) { return save_status; }
CommandResult PersistenceManager_RequestSave(void)
{ ++save_requests; return save_result; }
CommandResult PersistenceManager_RequestCandidateSave(
    const DeviceConfig *candidate, const DeviceConfig *original,
    bool allow_cs1237_change, uint32_t expected_revision)
{
    (void)original; (void)allow_cs1237_change;
    if (save_result != COMMAND_RESULT_ACCEPTED) return save_result;
    if (expected_revision != context.config_revision) return COMMAND_RESULT_BUSY;
    ++save_requests;
    context.config = *candidate;
    context.config_revision = Revision_Next(expected_revision);
    context.runtime.config_dirty = true;
    save_status = PERSISTENCE_STATUS_SAVING;
    save_busy = true;
    return COMMAND_RESULT_ACCEPTED;
}

void DisplayController_SetPage(DisplayPage page) { (void)page; }
DisplayPage DisplayController_GetPage(void) { return DISPLAY_PAGE_NET; }
void DisplayController_ShowMessage(const char text[6], uint32_t duration_ms)
{
    (void)duration_ms;
    (void)memcpy(last_message, text, 6U);
    last_message[6] = '\0';
}
bool DisplayController_SetTextPage(DisplayPage page, const char text[6])
{
    (void)page;
    (void)memcpy(last_page, text, 6U);
    last_page[6] = '\0';
    return true;
}
bool DisplayController_SetTextEditPage(DisplayPage page, const char text[6],
    uint8_t digit, bool visible)
{ (void)digit; (void)visible; return DisplayController_SetTextPage(page, text); }
bool DisplayController_SetNumericEditPage(DisplayPage page, int32_t count,
    uint8_t decimals, uint8_t digit, bool visible)
{ (void)page; (void)count; (void)decimals; (void)digit; (void)visible;
  return true; }
bool DisplayController_SetBrightness(uint8_t brightness)
{ (void)brightness; return true; }
bool UnitConverter_CountToMass(int64_t count, MassUnit unit,
    uint8_t decimals, MassValueUg *mass)
{ (void)unit; (void)decimals; *mass = count; return true; }
bool UnitConverter_MassToDisplay(MassValueUg mass, MassUnit unit,
    const UnitDisplayConfig *config, DisplayWeightValue *display)
{
    (void)unit; (void)config;
    display->display_count = (int32_t)mass;
    display->valid = true;
    display->overflow = false;
    return true;
}

static bool Key(KeyId key, KeyEventType type)
{
    KeyEvent event = {key, type, now_ms, 0U};
    now_ms += 50U;
    return MenuController_HandleKeyEvent(&event);
}

static int EnterAdvanced(MenuItem item)
{
    unsigned guard = 0U;
    CHECK(MenuController_Enter());
    CHECK(Key(KEY_ID_STAR, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_STAR, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(MenuController_IsAdvanced());
    while ((MenuController_GetItem() != item) && (guard++ < MENU_ITEM_COUNT))
        CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(MenuController_GetItem() == item);
    return 0;
}

static void Reset(void)
{
    if (MenuController_IsActive()) MenuController_Cancel();
    (void)memset(&context, 0, sizeof(context));
    (void)memset(last_message, 0, sizeof(last_message));
    (void)memset(last_page, 0, sizeof(last_page));
    context.config.metrology.active_profile = WEIGHING_PROFILE_HIGH_PRECISION;
    context.config.metrology.profiles[0].sample_rate =
        DEVICE_CS1237_DATA_RATE_10_HZ;
    context.config.metrology.profiles[0].filter_mode = FILTER_MODE_MEDIAN3_IIR;
    context.config.metrology.profiles[0].filter_strength = 3U;
    context.config_revision = 8U;
    context.saved_revision = 8U;
    context.initialized = true;
    save_requests = 0U;
    runtime_set_requests = 0U;
    save_result = COMMAND_RESULT_ACCEPTED;
    save_status = PERSISTENCE_STATUS_IDLE;
    save_busy = false;
    invalid_config = false;
    checkweigh_generation = 0U;
    now_ms = 100U;
    MenuController_Init();
}

static int TestR5SaveTiming(void)
{
    DeviceConfig candidate;
    Reset();
    CHECK(EnterAdvanced(MENU_ITEM_R5_DRIFT) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(memcmp(last_page, " SHAdO", 6U) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(save_requests == 0U && runtime_set_requests == 0U);
    CHECK(context.config.system.requested_r5_mode == 0U);
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(MenuController_GetItem() == MENU_ITEM_R5_DRIFT);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_LONG));
    CHECK(save_requests == 1U && runtime_set_requests == 0U);
    CHECK(context.config.system.requested_r5_mode == 2U);
    CHECK(context.runtime.config_dirty);
    CHECK(!IsDone());
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_LONG));
    CHECK(save_requests == 1U);
    context.saved_revision = context.config_revision;
    context.runtime.config_dirty = false;
    save_status = PERSISTENCE_STATUS_SUCCESS;
    save_busy = false;
    MenuController_Process10ms();
    CHECK(IsDone());
    CHECK(MenuController_GetCandidate(&candidate));
    CHECK(candidate.system.requested_r5_mode == 2U);
    return 0;
}

static int TestCancellationAndStale(void)
{
    Reset();
    CHECK(EnterAdvanced(MENU_ITEM_R5_DRIFT) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_TARE, KEY_EVENT_SHORT));
    CHECK(context.config.system.requested_r5_mode == 0U);
    CHECK(save_requests == 0U);
    Reset();
    CHECK(EnterAdvanced(MENU_ITEM_R5_DRIFT) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    ++context.config_revision;
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_LONG));
    CHECK(save_requests == 0U);
    CHECK(context.config.system.requested_r5_mode == 0U);
    Reset();
    CHECK(EnterAdvanced(MENU_ITEM_FILTER) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    now_ms += MENU_TIMEOUT_MS;
    MenuController_Process10ms();
    CHECK(!MenuController_IsActive());
    CHECK(save_requests == 0U);
    CHECK(context.config.metrology.profiles[0].filter_mode ==
        FILTER_MODE_MEDIAN3_IIR);
    return 0;
}

static int TestProfileFields(void)
{
    DeviceConfig candidate;
    Reset();
    CHECK(EnterAdvanced(MENU_ITEM_FILTER) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(memcmp(last_page, " FILt0", 6U) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(MenuController_GetCandidate(&candidate));
    CHECK(candidate.metrology.profiles[0].filter_mode == FILTER_MODE_NONE);
    CHECK(candidate.metrology.profiles[0].filter_strength == 3U);
    CHECK(Key(KEY_ID_TARE, KEY_EVENT_SHORT));
    Reset();
    CHECK(EnterAdvanced(MENU_ITEM_FILTER_STRENGTH) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(MenuController_GetCandidate(&candidate));
    CHECK(candidate.metrology.profiles[0].filter_strength == 4U);
    CHECK(candidate.metrology.profiles[0].filter_mode ==
        FILTER_MODE_MEDIAN3_IIR);
    CHECK(candidate.metrology.profiles[0].sample_rate ==
        DEVICE_CS1237_DATA_RATE_10_HZ);
    CHECK(Key(KEY_ID_TARE, KEY_EVENT_SHORT));
    Reset();
    CHECK(EnterAdvanced(MENU_ITEM_SAMPLE_RATE) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(memcmp(last_page, "  10Hz", 6U) == 0);
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(memcmp(last_page, "  40Hz", 6U) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(MenuController_GetCandidate(&candidate));
    CHECK(candidate.metrology.profiles[0].sample_rate ==
        DEVICE_CS1237_DATA_RATE_40_HZ);
    CHECK(candidate.metrology.profiles[0].filter_strength == 3U);
    return 0;
}

static int TestCheckweighAndSaveFailure(void)
{
    Reset();
    CHECK(EnterAdvanced(MENU_ITEM_CHECKWEIGH_MODE) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    ++checkweigh_generation;
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_LONG));
    CHECK(save_requests == 0U);
    Reset();
    CHECK(EnterAdvanced(MENU_ITEM_CHECKWEIGH_MODE) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_LONG));
    CHECK(save_requests == 1U);
    CHECK(!IsDone());
    save_status = PERSISTENCE_STATUS_FAILED;
    save_busy = false;
    MenuController_Process10ms();
    CHECK(!IsDone());
    return 0;
}

static int TestSaveRejectionAndNoChange(void)
{
    Reset();
    CHECK(EnterAdvanced(MENU_ITEM_FILTER) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_HASH, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    save_busy = true;
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_LONG));
    CHECK(save_requests == 0U && context.config_revision == 8U);
    save_busy = false;
    save_result = COMMAND_RESULT_POWER_UNSAFE;
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_LONG));
    CHECK(save_requests == 0U && context.config_revision == 8U);
    CHECK(!IsDone());
    invalid_config = true;
    save_result = COMMAND_RESULT_ACCEPTED;
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_LONG));
    CHECK(save_requests == 0U && context.config_revision == 8U);
    CHECK(!IsDone());
    Reset();
    CHECK(EnterAdvanced(MENU_ITEM_R5_DRIFT) == 0);
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_SHORT));
    CHECK(Key(KEY_ID_FUNCTION, KEY_EVENT_LONG));
    CHECK(save_requests == 0U && context.config_revision == 8U);
    CHECK(!IsDone());
    return 0;
}

int main(void)
{
    if (TestR5SaveTiming() != 0 || TestCancellationAndStale() != 0 ||
        TestProfileFields() != 0 || TestCheckweighAndSaveFailure() != 0 ||
        TestSaveRejectionAndNoChange() != 0)
        return 1;
    (void)puts("Stage 5P-A2C menu transaction tests passed");
    return 0;
}
