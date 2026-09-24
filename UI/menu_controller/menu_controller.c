#include "menu_controller.h"
#include "revision_helper.h"
#include "persistent_codec.h"

#include "bsp_time.h"
#include "command_service.h"
#include "config_application.h"
#include "config_edit.h"
#include "display_controller.h"
#include "display_codes.h"
#include "mass_math.h"
#include "metrology_config_validator.h"
#include "metrology_manager.h"
#include "persistence_manager.h"
#include "numeric_edit_cursor.h"
#include "project_config.h"
#include "system_context.h"
#include "unit_converter.h"
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
#include "r5_local_control.h"
#include "ui_config_workspace.h"
#endif
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
#include "checkweigh_local_control.h"
#endif

#include <limits.h>
#include <stddef.h>
#include <string.h>

typedef enum
{
    MENU_EDIT_NONE = 0,
    MENU_EDIT_UNIT,
    MENU_EDIT_PROFILE,
    MENU_EDIT_INTEGER,
    MENU_EDIT_MASS,
    MENU_EDIT_UNIT_DISPLAY,
    MENU_EDIT_FILTER,
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
    MENU_EDIT_SAMPLE_RATE,
    MENU_EDIT_FILTER_STRENGTH,
#endif
    MENU_EDIT_STABILITY_HOLD,
    MENU_EDIT_BOOL
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    ,
    MENU_EDIT_R5_DRIFT
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    ,
    MENU_EDIT_CHECKWEIGH_MODE
#endif
#endif
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
    ,
    MENU_EDIT_ALARM_SOURCE
#endif
} MenuEditKind;

static const char s_labels[MENU_ITEM_COUNT][6] = {
    {'U','n','I','t',' ',' '}, {'P','r','O','F',' ',' '},
    {'C','A','L',' ',' ',' '}, {'C','A','P',' ',' ',' '},
    {'d','I','U',' ',' ',' '}, {'d','P',' ',' ',' ',' '},
    {'F','I','L','t',' ',' '},
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
    {'S','t','r','E','n','G'},
#endif
    {'S','t','A','b',' ',' '},
    {'Z','r','n','G',' ',' '}, {'P','-','Z','r',' ',' '},
    {'O','L',' ',' ',' ',' '},
    {'b','r','I','G','H','t'}, {'S','P','d',' ',' ',' '},
    {'G','A','I','n',' ',' '}, {'t','r','r','E','t',' '},
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    {'d','r','I','F','t',' '},
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    {'A','L','A','r','n',' '},
#endif
#endif
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
    {'L','-','E','n',' ',' '}, {'L','o',' ',' ',' ',' '},
    {'H','i',' ',' ',' ',' '}, {'H','y','S',' ',' ',' '},
    {'S','r','c',' ',' ',' '}, {'b','I','n',' ',' ',' '},
    {'b','E','H',' ',' ',' '}, {'b','O','K',' ',' ',' '},
#endif
    {'S','A','U','E',' ',' '}, {'r','E','S','E','t',' '},
    {'E','H','I','t',' ',' '}
};

static const MenuItem s_ordinary[] = {
    MENU_ITEM_UNIT, MENU_ITEM_PROFILE, MENU_ITEM_BRIGHTNESS,
    MENU_ITEM_TARE_RETENTION};

static MenuItem s_item;
static MenuEditKind s_edit_kind;
static ConfigFieldId s_integer_field;
static ConfigMassFieldId s_mass_field;
static int64_t s_value;
static NumericEditCursor s_edit_cursor;
static MassUnit s_edit_unit;
static WeighingProfileId s_edit_profile;
static UnitDisplayConfig s_edit_display;
static MassUnit s_original_unit;
static MassUnit s_candidate_unit;
static DisplayCode s_begin_error;
static bool s_begin_warning;
static uint32_t s_last_activity_ms;
static bool s_active;
static bool s_editing;
static bool s_calibration_request;
static bool s_exit_request;
static bool s_factory_confirmation;
static bool s_advanced;
static KeyId s_sequence_keys[4];
static uint8_t s_sequence_count;
static uint32_t s_sequence_start_ms;
static uint32_t s_sequence_last_ms;
static uint32_t s_expected_revision;
static uint32_t s_save_revision;
static uint32_t s_save_started_ms;
static uint32_t s_message_until_ms;
static bool s_entry_ownership_allowed;
static bool s_save_waiting;
static bool s_exit_after_save;
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
#define s_original_config (*UiConfigWorkspace_Original())
#define s_candidate_config (*UiConfigWorkspace_Candidate())
#else
static DeviceConfig s_original_config;
static DeviceConfig s_candidate_config;
#endif
static DisplayPage s_previous_page;
static bool s_candidate_changed;
static bool s_brightness_previewed;
static bool s_existing_dirty_owned;
static uint32_t s_existing_dirty_revision;
#if defined(STAGE2A_HOST_TEST)
static uint32_t s_cancel_request_count;
#endif

static CommandResult MenuController_Command(CommandId id, int32_t value0,
    int32_t value1, uint32_t flags, int64_t value64)
{
    CommandRequest request = {0};
    CommandResponse response;
    request.id = id;
    request.source = COMMAND_SOURCE_LOCAL_KEY;
    request.value0 = value0;
    request.value1 = value1;
    request.flags = flags;
    request.value64 = value64;
    return CommandService_Execute(&request, &response);
}

static void ShowCode(DisplayCode code)
{
    char text[6];
    if (DisplayCodes_Get(code, text))
        DisplayController_ShowMessage(text, UI_MESSAGE_DEFAULT_MS);
}

static void FormatInteger(int64_t value, char text[6])
{
    uint64_t magnitude = (value < 0) ?
        (UINT64_C(0) - (uint64_t)value) : (uint64_t)value;
    uint8_t index;
    for (index = 0U; index < 6U; ++index) text[index] = ' ';
    index = 5U;
    do
    {
        text[index] = (char)('0' + (magnitude % 10U));
        magnitude /= 10U;
        if (index == 0U) break;
        --index;
    } while (magnitude != 0U);
    if ((value < 0) && (index > 0U)) text[index] = '-';
}

static void Render(void)
{
    char text[6];
    if (!s_editing)
    {
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_MENU,
                                            s_labels[s_item]);
        return;
    }
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    if (s_edit_kind == MENU_EDIT_R5_DRIFT)
    {
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_EDIT,
            R5LocalControl_ChoiceText(R5LocalControl_GetChoice()));
        return;
    }
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    if (s_edit_kind == MENU_EDIT_CHECKWEIGH_MODE)
    {
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_EDIT,
            CheckweighLocalControl_ChoiceText(
                CheckweighLocalControl_GetChoice()));
        return;
    }
#endif
#endif
    if (s_edit_kind == MENU_EDIT_UNIT)
    {
        if (!DisplayCodes_GetMassUnitLabel(s_candidate_unit, text) ||
            !DisplayController_SetTextPage(DISPLAY_PAGE_EDIT, text))
            ShowCode(DISPLAY_CODE_UNIT_ERROR);
        return;
    }
    if (s_edit_kind == MENU_EDIT_BOOL)
    {
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_EDIT,
            (s_value != 0) ? "    On" : "   OFF");
        return;
    }
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
    if (s_edit_kind == MENU_EDIT_SAMPLE_RATE)
    {
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_EDIT,
            (s_value == DEVICE_CS1237_DATA_RATE_10_HZ) ? "  10Hz" : "  40Hz");
        return;
    }
    if (s_edit_kind == MENU_EDIT_FILTER)
    {
        static const char choices[FILTER_MODE_COUNT][7] = {
            " FILt0", " FILt1", " FILt2", " FILt3"};
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_EDIT,
            choices[(uint32_t)s_value < FILTER_MODE_COUNT ? s_value : 0]);
        return;
    }
#endif
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
    if (s_edit_kind == MENU_EDIT_ALARM_SOURCE)
    {
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_EDIT,
            (s_value == ALARM_WEIGHT_GROSS) ? " GroSS" : "   nEt");
        return;
    }
#endif
    if ((s_edit_kind == MENU_EDIT_MASS) &&
        (s_value <= INT32_MAX) && (s_value >= INT32_MIN))
    {
        (void)DisplayController_SetNumericEditPage(DISPLAY_PAGE_EDIT,
            (int32_t)s_value, s_edit_display.decimal_places,
            s_edit_cursor.selected_digit, s_edit_cursor.visible);
    }
    else
    {
        FormatInteger(s_value, text);
        if (s_edit_kind == MENU_EDIT_STABILITY_HOLD)
            (void)DisplayController_SetTextEditPage(DISPLAY_PAGE_EDIT, text,
                s_edit_cursor.selected_digit, s_edit_cursor.visible);
        else
            (void)DisplayController_SetTextPage(DISPLAY_PAGE_EDIT, text);
    }
}

static void ClearSequence(void)
{
    s_sequence_count = 0U;
}

static void ExitMenu(void)
{
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    R5LocalControl_EndSession();
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    CheckweighLocalControl_EndSession();
#endif
#endif
    s_active = false;
    s_editing = false;
    s_save_waiting = false;
    s_factory_confirmation = false;
    ClearSequence();
    DisplayController_SetPage(s_previous_page);
    s_exit_request = true;
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    UiConfigWorkspace_Release(UI_CONFIG_WORKSPACE_MENU);
#endif
}

static void CancelUnconfirmedEdit(void)
{
    if (s_editing && (s_item == MENU_ITEM_BRIGHTNESS))
        (void)DisplayController_SetBrightness(
            s_candidate_config.display.brightness);
#if defined(STAGE2A_HOST_TEST)
    if (s_editing && (s_edit_kind != MENU_EDIT_UNIT))
        ++s_cancel_request_count;
#endif
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    if (s_editing && (s_edit_kind == MENU_EDIT_R5_DRIFT))
        R5LocalControl_Cancel();
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    if (s_editing && (s_edit_kind == MENU_EDIT_CHECKWEIGH_MODE))
        CheckweighLocalControl_Cancel();
#endif
#endif
    s_editing = false;
}

static void RestoreOriginalBrightness(void)
{
    if (s_brightness_previewed)
        (void)DisplayController_SetBrightness(
            s_original_config.display.brightness);
}

static void DiscardCandidate(void)
{
    CancelUnconfirmedEdit();
    RestoreOriginalBrightness();
    s_brightness_previewed = false;
    s_candidate_config = s_original_config;
    s_candidate_changed = false;
}

static void RequestSave(uint32_t now_ms)
{
    CommandResult result;
    CancelUnconfirmedEdit();
    if (!s_candidate_changed)
    {
        if (SystemContext_GetConfigRevision() ==
            SystemContext_GetSavedRevision())
        {
            ShowCode(DISPLAY_CODE_NO_CHANGE);
            s_exit_after_save = true;
            s_message_until_ms = now_ms + UI_MESSAGE_DEFAULT_MS;
            return;
        }
        if (!s_existing_dirty_owned ||
            (SystemContext_GetConfigRevision() != s_existing_dirty_revision))
        {
            RestoreOriginalBrightness();
            ShowCode(DISPLAY_CODE_BUSY);
            return;
        }
        result = PersistenceManager_RequestSave();
        if (result != COMMAND_RESULT_ACCEPTED)
        {
            RestoreOriginalBrightness();
            ShowCode(DISPLAY_CODE_SAVE_ERROR);
            return;
        }
        s_save_revision = s_existing_dirty_revision;
        s_save_started_ms = now_ms;
        s_save_waiting = true;
        s_exit_after_save = true;
        ShowCode(DISPLAY_CODE_SAVE);
        return;
    }
    /* The store writes a complete snapshot. A menu session must neither claim
       pre-existing dirty state nor overwrite a revision from another owner. */
    if (!s_entry_ownership_allowed ||
        (SystemContext_GetConfigRevision() != s_expected_revision) ||
        ((SystemContext_GetSavedRevision() != s_expected_revision) &&
         (!s_existing_dirty_owned ||
          (s_existing_dirty_revision != s_expected_revision))))
    {
        RestoreOriginalBrightness();
        ShowCode(DISPLAY_CODE_BUSY);
        return;
    }
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
    if (PersistenceManager_IsBusy())
    {
        RestoreOriginalBrightness();
        ShowCode(DISPLAY_CODE_BUSY);
        return;
    }
#endif
    if (ConfigApplication_Validate(&s_candidate_config, true) !=
        CONFIG_APPLY_OK)
    {
        RestoreOriginalBrightness();
        ShowCode(DISPLAY_CODE_INVALID_CONFIG);
        return;
    }
    result = PersistenceManager_RequestCandidateSave(&s_candidate_config,
        &s_original_config, true, s_expected_revision);
    if (result != COMMAND_RESULT_ACCEPTED)
    {
        RestoreOriginalBrightness();
        ShowCode(result == COMMAND_RESULT_BUSY ? DISPLAY_CODE_BUSY :
                 DISPLAY_CODE_SAVE_ERROR);
        return;
    }
    s_save_revision = Revision_Next(s_expected_revision);
    s_save_started_ms = now_ms;
    s_save_waiting = true;
    s_exit_after_save = true;
    ShowCode(DISPLAY_CODE_SAVE);
}

#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
static void RequestR5CandidateSave(uint32_t now_ms)
{
    R5LocalStatus status;
    if ((SystemContext_GetConfigRevision() != s_expected_revision) ||
        !R5LocalControl_GetStatus(&status) ||
        (status.application != (R5LocalApplication)
            s_original_config.system.requested_r5_application) ||
        (status.mode != (R5DriftMode)
            s_original_config.system.requested_r5_mode))
    {
        ShowCode(DISPLAY_CODE_BUSY);
        return;
    }
    switch (R5LocalControl_GetChoice())
    {
        case R5_LOCAL_CHOICE_OFF:
            s_candidate_config.system.requested_r5_application = 0U;
            s_candidate_config.system.requested_r5_mode = 0U;
            break;
        case R5_LOCAL_CHOICE_SHADOW:
            s_candidate_config.system.requested_r5_application = 0U;
            s_candidate_config.system.requested_r5_mode = 2U;
            break;
        case R5_LOCAL_CHOICE_STATIC:
            s_candidate_config.system.requested_r5_application = 1U;
            s_candidate_config.system.requested_r5_mode = 2U;
            break;
        case R5_LOCAL_CHOICE_DOSING:
            s_candidate_config.system.requested_r5_application = 1U;
            s_candidate_config.system.requested_r5_mode = 1U;
            break;
        default:
            ShowCode(DISPLAY_CODE_INVALID_CONFIG);
            return;
    }
    s_candidate_changed = !PersistentCodec_DeviceConfigEqual(
        &s_candidate_config, &s_original_config);
    RequestSave(now_ms);
}
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
static void RequestCheckweighCandidateSave(uint32_t now_ms)
{
    if ((SystemContext_GetConfigRevision() != s_expected_revision) ||
        !CheckweighLocalControl_CandidateCurrent())
    {
        ShowCode(DISPLAY_CODE_BUSY);
        return;
    }
    s_candidate_config.system.requested_checkweigh_mode =
        (uint8_t)CheckweighLocalControl_GetChoice();
    s_candidate_changed = !PersistentCodec_DeviceConfigEqual(
        &s_candidate_config, &s_original_config);
    RequestSave(now_ms);
}
#endif
#endif

static void Navigate(KeyId key)
{
    if (s_advanced)
    {
        s_item = (key == KEY_ID_HASH) ?
            (MenuItem)(((uint32_t)s_item + 1U) % MENU_ITEM_COUNT) :
            (MenuItem)(((uint32_t)s_item + MENU_ITEM_COUNT - 1U) %
                       MENU_ITEM_COUNT);
        while (
#if (A33_ENABLE_STAGE5PA2C_PRODUCT == 0U)
               (s_item == MENU_ITEM_SAMPLE_RATE) ||
#endif
               (s_item == MENU_ITEM_GAIN) ||
               (s_item == MENU_ITEM_SAVE) ||
               (s_item == MENU_ITEM_EXIT))
        {
            s_item = (key == KEY_ID_HASH) ?
                (MenuItem)(((uint32_t)s_item + 1U) % MENU_ITEM_COUNT) :
                (MenuItem)(((uint32_t)s_item + MENU_ITEM_COUNT - 1U) %
                           MENU_ITEM_COUNT);
        }
    }
    else
    {
        uint8_t index;
        const uint8_t count = (uint8_t)(sizeof(s_ordinary) /
                                        sizeof(s_ordinary[0]));
        for (index = 0U; index < count; ++index)
            if (s_ordinary[index] == s_item) break;
        index = (key == KEY_ID_HASH) ?
            (uint8_t)((index + 1U) % count) :
            (uint8_t)((index + count - 1U) % count);
        s_item = s_ordinary[index];
    }
}

static void ReplaySequence(void)
{
    KeyId keys[4];
    uint8_t count = s_sequence_count;
    uint8_t index;
    for (index = 0U; index < count; ++index) keys[index] = s_sequence_keys[index];
    ClearSequence();
    for (index = 0U; index < count; ++index) Navigate(keys[index]);
    Render();
}

static bool HandleAdvancedSequence(const KeyEvent *event)
{
    static const KeyId expected[4] = {
        KEY_ID_STAR, KEY_ID_HASH, KEY_ID_STAR, KEY_ID_HASH};
    if (s_advanced || s_editing || (s_item != MENU_ITEM_UNIT))
    {
        ClearSequence();
        return false;
    }
    if (event->type != KEY_EVENT_SHORT)
    {
        if (s_sequence_count != 0U) ReplaySequence();
        return false;
    }
    if ((event->key != KEY_ID_STAR) && (event->key != KEY_ID_HASH))
    {
        if (s_sequence_count != 0U) ReplaySequence();
        return false;
    }
    if (s_sequence_count == 0U)
    {
        if (event->key == KEY_ID_HASH) return false;
        s_sequence_keys[0] = KEY_ID_STAR;
        s_sequence_count = 1U;
        s_sequence_start_ms = event->timestamp_ms;
        s_sequence_last_ms = event->timestamp_ms;
        return true;
    }
    if (((uint32_t)(event->timestamp_ms - s_sequence_last_ms) > 1000U) ||
        ((uint32_t)(event->timestamp_ms - s_sequence_start_ms) > 4000U))
    {
        ReplaySequence();
        return false;
    }
    s_sequence_keys[s_sequence_count] = event->key;
    ++s_sequence_count;
    s_sequence_last_ms = event->timestamp_ms;
    if (event->key != expected[s_sequence_count - 1U])
    {
        ReplaySequence();
        return true;
    }
    if (s_sequence_count == 4U)
    {
        ClearSequence();
        s_advanced = true;
        s_item = MENU_ITEM_CAPACITY;
        Render();
    }
    return true;
}

static bool BeginEdit(MenuItem item, uint32_t now_ms)
{
    const SystemContext *context = SystemContext_Get();
    const MetrologyConfig *metrology;
    const WeighingProfileConfig *profile;
    DisplayWeightValue display_value;
    MassValueUg mass = 0;
    char unit_label[6];
    if (context == NULL) return false;
    s_begin_error = DISPLAY_CODE_BUSY;
    if (SystemContext_GetConfigRevision() != s_expected_revision)
        return false;
    s_begin_warning = false;
    metrology = &s_candidate_config.metrology;
    s_edit_unit = metrology->active_unit;
    s_edit_profile = metrology->active_profile;
    s_edit_display = metrology->unit_display[s_edit_unit];
    profile = &metrology->profiles[s_edit_profile];
    NumericEditCursor_Init(&s_edit_cursor, now_ms);
    switch (item)
    {
        case MENU_ITEM_UNIT:
            s_edit_kind = MENU_EDIT_UNIT;
            s_original_unit = metrology->active_unit;
            s_candidate_unit = metrology->active_unit;
            if (!DisplayCodes_GetMassUnitLabel(s_candidate_unit, unit_label) ||
                !DisplayController_SetTextPage(DISPLAY_PAGE_EDIT, unit_label))
            {
                s_begin_error = DISPLAY_CODE_UNIT_ERROR;
                return false;
            }
            s_editing = true;
            return true;
        case MENU_ITEM_PROFILE:
            s_edit_kind = MENU_EDIT_PROFILE;
            s_value = metrology->active_profile;
            s_editing = true;
            Render();
            return true;
        case MENU_ITEM_CAPACITY:
            s_edit_kind = MENU_EDIT_MASS;
            s_mass_field = CONFIG_MASS_FIELD_CAPACITY;
            mass = metrology->capacity_ug;
            break;
        case MENU_ITEM_ZERO_RANGE:
            s_edit_kind = MENU_EDIT_MASS;
            s_mass_field = CONFIG_MASS_FIELD_ZERO_RANGE;
            mass = metrology->zero_range_ug;
            break;
        case MENU_ITEM_STARTUP_AUTO_ZERO:
            s_edit_kind = MENU_EDIT_BOOL;
            s_integer_field = CONFIG_FIELD_STARTUP_AUTO_ZERO_ENABLE;
            s_value = s_candidate_config.system.startup_auto_zero_enable ? 1 : 0;
            break;
        case MENU_ITEM_OVERLOAD:
            if (metrology->compliance_mode ==
                METROLOGY_COMPLIANCE_CLASS_III_REFERENCE)
                return false;
            s_edit_kind = MENU_EDIT_MASS;
            s_mass_field = CONFIG_MASS_FIELD_OVERLOAD_THRESHOLD;
            mass = metrology->overload_threshold_ug;
            break;
        case MENU_ITEM_DIVISION:
        case MENU_ITEM_DECIMALS:
            s_edit_kind = MENU_EDIT_UNIT_DISPLAY;
            s_value = (item == MENU_ITEM_DIVISION) ?
                s_edit_display.division_digit : s_edit_display.decimal_places;
            break;
        case MENU_ITEM_FILTER:
            s_edit_kind = MENU_EDIT_FILTER;
            s_value = profile->filter_mode;
            break;
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
        case MENU_ITEM_FILTER_STRENGTH:
            s_edit_kind = MENU_EDIT_FILTER_STRENGTH;
            s_value = profile->filter_strength;
            break;
        case MENU_ITEM_SAMPLE_RATE:
            s_edit_kind = MENU_EDIT_SAMPLE_RATE;
            s_value = profile->sample_rate;
            break;
#endif
        case MENU_ITEM_STABILITY:
            s_edit_kind = MENU_EDIT_STABILITY_HOLD;
            s_value = profile->stability_hold_ms;
            break;
        case MENU_ITEM_BRIGHTNESS:
            s_edit_kind = MENU_EDIT_INTEGER;
            s_integer_field = CONFIG_FIELD_DISPLAY_BRIGHTNESS;
            s_value = s_candidate_config.display.brightness;
            break;
        case MENU_ITEM_TARE_RETENTION:
            s_edit_kind = MENU_EDIT_BOOL;
            s_integer_field = CONFIG_FIELD_TARE_RETENTION;
            s_value = s_candidate_config.system.tare_power_loss_retention ? 1 : 0;
            break;
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
        case MENU_ITEM_R5_DRIFT:
            if (!R5LocalControl_Begin(s_candidate_changed)) return false;
            s_edit_kind = MENU_EDIT_R5_DRIFT;
            s_editing = true;
            Render();
            return true;
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
        case MENU_ITEM_CHECKWEIGH_MODE:
            if (!CheckweighLocalControl_Begin(s_candidate_changed)) return false;
            s_edit_kind = MENU_EDIT_CHECKWEIGH_MODE;
            s_editing = true;
            Render();
            return true;
#endif
#endif
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
        case MENU_ITEM_LIMIT_ENABLE:
            s_edit_kind = MENU_EDIT_BOOL;
            s_integer_field = CONFIG_FIELD_LIMIT_ENABLE;
            s_value = s_candidate_config.alarm.limit_function_enable ? 1 : 0;
            break;
        case MENU_ITEM_ALARM_LOWER_LIMIT:
            s_edit_kind = MENU_EDIT_MASS;
            s_mass_field = CONFIG_MASS_FIELD_ALARM_LOWER_LIMIT;
            mass = s_candidate_config.alarm.lower_limit_ug;
            break;
        case MENU_ITEM_ALARM_UPPER_LIMIT:
            s_edit_kind = MENU_EDIT_MASS;
            s_mass_field = CONFIG_MASS_FIELD_ALARM_UPPER_LIMIT;
            mass = s_candidate_config.alarm.upper_limit_ug;
            break;
        case MENU_ITEM_ALARM_HYSTERESIS:
            s_edit_kind = MENU_EDIT_MASS;
            s_mass_field = CONFIG_MASS_FIELD_ALARM_HYSTERESIS;
            mass = s_candidate_config.alarm.hysteresis_ug;
            break;
        case MENU_ITEM_ALARM_SOURCE:
            s_edit_kind = MENU_EDIT_ALARM_SOURCE;
            s_integer_field = CONFIG_FIELD_ALARM_WEIGHT_SOURCE;
            s_value = s_candidate_config.alarm.weight_source;
            break;
        case MENU_ITEM_INTERNAL_BUZZER:
            s_edit_kind = MENU_EDIT_BOOL;
            s_integer_field = CONFIG_FIELD_INTERNAL_BUZZER_ENABLE;
            s_value = s_candidate_config.alarm.internal_buzzer_enable ? 1 : 0;
            break;
        case MENU_ITEM_EXTERNAL_BUZZER:
            s_edit_kind = MENU_EDIT_BOOL;
            s_integer_field = CONFIG_FIELD_EXTERNAL_BUZZER_ENABLE;
            s_value = s_candidate_config.alarm.external_buzzer_enable ? 1 : 0;
            break;
        case MENU_ITEM_QUALIFIED_BEEP:
            s_edit_kind = MENU_EDIT_BOOL;
            s_integer_field = CONFIG_FIELD_QUALIFIED_BEEP_ENABLE;
            s_value = s_candidate_config.alarm.qualified_beep_enable ? 1 : 0;
            break;
#endif
        default: return false;
    }
    if (s_edit_kind == MENU_EDIT_MASS)
    {
        if (!UnitConverter_MassToDisplay(mass, s_edit_unit, &s_edit_display,
                                         &display_value) ||
            !display_value.valid || display_value.overflow)
        {
            if ((item != MENU_ITEM_OVERLOAD) ||
                !UnitConverter_MassToDisplay(metrology->capacity_ug,
                    s_edit_unit, &s_edit_display, &display_value) ||
                !display_value.valid || display_value.overflow)
            {
                s_begin_error = DISPLAY_CODE_UNIT_RANGE;
                return false;
            }
            s_begin_warning = true;
        }
        s_value = display_value.display_count;
    }
    s_editing = true;
    Render();
    if (s_begin_warning) ShowCode(DISPLAY_CODE_UNIT_RANGE);
    return true;
}

static bool SubmitEditValue(void)
{
    MassValueUg mass;
#if (A33_ENABLE_STAGE5PA2C_PRODUCT == 0U)
    uint8_t strength;
#endif
    bool updated = false;
    if (s_edit_kind == MENU_EDIT_UNIT)
    {
        s_candidate_config.metrology.active_unit = s_candidate_unit;
        return true;
    }
    if (s_edit_kind == MENU_EDIT_PROFILE)
    {
        s_candidate_config.metrology.active_profile =
            (WeighingProfileId)s_value;
        return true;
    }
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    if (s_edit_kind == MENU_EDIT_R5_DRIFT)
    {
        R5LocalControl_Confirm();
        return true;
    }
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    if (s_edit_kind == MENU_EDIT_CHECKWEIGH_MODE)
    {
        CheckweighLocalControl_Confirm();
        return true;
    }
#endif
#endif
    if (!ConfigEdit_Begin(&s_candidate_config)) return false;
    switch (s_edit_kind)
    {
        case MENU_EDIT_INTEGER:
        case MENU_EDIT_BOOL:
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
        case MENU_EDIT_ALARM_SOURCE:
#endif
            updated = ConfigEdit_SetIntegerField(s_integer_field,
                                                  (int32_t)s_value);
            break;
        case MENU_EDIT_MASS:
            updated = UnitConverter_CountToMass(s_value, s_edit_unit,
                s_edit_display.decimal_places, &mass) &&
                ConfigEdit_SetMassField(s_mass_field, mass);
            break;
        case MENU_EDIT_UNIT_DISPLAY:
            updated = ConfigEdit_SetUnitDisplay(s_edit_unit, &s_edit_display);
            break;
        case MENU_EDIT_FILTER:
#if (A33_ENABLE_STAGE5PA2C_PRODUCT == 0U)
            strength = (s_value == FILTER_MODE_NONE) ? 0U :
                       (s_value == FILTER_MODE_AVERAGE) ? 2U : 1U;
            updated = ConfigEdit_SetProfileField(s_edit_profile,
                CONFIG_PROFILE_FIELD_FILTER_MODE, s_value) &&
                ConfigEdit_SetProfileField(s_edit_profile,
                    CONFIG_PROFILE_FIELD_FILTER_STRENGTH, strength);
#else
            updated = ConfigEdit_SetProfileField(s_edit_profile,
                CONFIG_PROFILE_FIELD_FILTER_MODE, s_value);
#endif
            break;
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
        case MENU_EDIT_FILTER_STRENGTH:
            updated = ConfigEdit_SetProfileField(s_edit_profile,
                CONFIG_PROFILE_FIELD_FILTER_STRENGTH, s_value);
            break;
        case MENU_EDIT_SAMPLE_RATE:
            updated = ConfigEdit_SetProfileField(s_edit_profile,
                CONFIG_PROFILE_FIELD_SAMPLE_RATE, s_value);
            break;
#endif
        case MENU_EDIT_STABILITY_HOLD:
            updated = ConfigEdit_SetProfileField(s_edit_profile,
                CONFIG_PROFILE_FIELD_STABILITY_HOLD_MS, s_value);
            break;
        case MENU_EDIT_NONE:
        default: break;
    }
    if (updated) updated = ConfigEdit_CopyWorking(&s_candidate_config);
    ConfigEdit_Cancel();
    return updated;
}

static void AdjustEdit(KeyId key)
{
    int64_t delta;
    int64_t next;
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    if (s_edit_kind == MENU_EDIT_R5_DRIFT)
    {
        R5LocalControl_Adjust(key == KEY_ID_HASH);
        return;
    }
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    if (s_edit_kind == MENU_EDIT_CHECKWEIGH_MODE)
    {
        CheckweighLocalControl_Adjust(key == KEY_ID_HASH);
        return;
    }
#endif
#endif
    if (s_edit_kind == MENU_EDIT_BOOL)
    {
        s_value = (s_value == 0) ? 1 : 0;
        return;
    }
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
    if (s_edit_kind == MENU_EDIT_ALARM_SOURCE)
    {
        s_value = (s_value == ALARM_WEIGHT_NET) ?
            ALARM_WEIGHT_GROSS : ALARM_WEIGHT_NET;
        return;
    }
#endif
    if (s_edit_kind == MENU_EDIT_UNIT)
    {
        const SystemContext *context = SystemContext_Get();
        uint8_t offset;
        if (context == NULL) return;
        for (offset = 1U; offset <= MASS_UNIT_COUNT; ++offset)
        {
            uint32_t candidate = (key == KEY_ID_HASH) ?
                ((uint32_t)s_candidate_unit + offset) % MASS_UNIT_COUNT :
                ((uint32_t)s_candidate_unit + MASS_UNIT_COUNT - offset) %
                    MASS_UNIT_COUNT;
            if (((s_candidate_config.metrology.enabled_unit_mask &
                  (uint8_t)(1U << candidate)) != 0U) &&
                !((s_candidate_config.metrology.compliance_mode ==
                   METROLOGY_COMPLIANCE_CLASS_III_REFERENCE) &&
                  (candidate == MASS_UNIT_LB)))
            {
                s_candidate_unit = (MassUnit)candidate;
                break;
            }
        }
        return;
    }
    if (s_edit_kind == MENU_EDIT_UNIT_DISPLAY)
    {
        if (s_item == MENU_ITEM_DIVISION)
        {
            static const uint8_t forward[3] = {2U, 5U, 1U};
            static const uint8_t backward[3] = {5U, 1U, 2U};
            uint8_t index = (s_edit_display.division_digit == 1U) ? 0U :
                (s_edit_display.division_digit == 2U) ? 1U : 2U;
            s_edit_display.division_digit = (key == KEY_ID_HASH) ?
                forward[index] : backward[index];
            s_value = s_edit_display.division_digit;
        }
        else
        {
            s_edit_display.decimal_places = (key == KEY_ID_HASH) ?
                (uint8_t)((s_edit_display.decimal_places + 1U) % 6U) :
                (uint8_t)((s_edit_display.decimal_places + 5U) % 6U);
            s_value = s_edit_display.decimal_places;
        }
        return;
    }
    if (s_edit_kind == MENU_EDIT_FILTER)
    {
        int32_t value = (int32_t)s_value +
            ((key == KEY_ID_HASH) ? 1 : -1);
        s_value = (value + FILTER_MODE_COUNT) % FILTER_MODE_COUNT;
        return;
    }
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
    if (s_edit_kind == MENU_EDIT_SAMPLE_RATE)
    {
        s_value = (s_value == DEVICE_CS1237_DATA_RATE_10_HZ) ?
            DEVICE_CS1237_DATA_RATE_40_HZ : DEVICE_CS1237_DATA_RATE_10_HZ;
        return;
    }
    if (s_edit_kind == MENU_EDIT_FILTER_STRENGTH)
    {
        uint8_t minimum;
        uint8_t maximum;
        FilterMode mode = s_candidate_config.metrology.profiles[
            s_edit_profile].filter_mode;
        if (!MetrologyConfig_FilterStrengthBounds(mode, &minimum, &maximum))
            return;
        s_value = (key == KEY_ID_HASH) ?
            ((s_value < minimum) || (s_value >= maximum) ? minimum :
                s_value + 1) :
            ((s_value <= minimum) || (s_value > maximum) ? maximum :
                s_value - 1);
        return;
    }
#endif
    if (s_edit_kind == MENU_EDIT_PROFILE)
    {
        s_value = (s_value == WEIGHING_PROFILE_HIGH_PRECISION) ?
            WEIGHING_PROFILE_HIGH_SPEED : WEIGHING_PROFILE_HIGH_PRECISION;
        return;
    }
    if (s_item == MENU_ITEM_BRIGHTNESS)
    {
        s_value = (key == KEY_ID_HASH) ?
            ((s_value >= 7) ? 1 : s_value + 1) :
            ((s_value <= 1) ? 7 : s_value - 1);
        (void)DisplayController_SetBrightness((uint8_t)s_value);
        s_brightness_previewed = true;
        return;
    }
    delta = (s_edit_kind == MENU_EDIT_MASS) ?
        (int64_t)s_edit_display.division_digit *
            NumericEditCursor_GetStep(&s_edit_cursor) :
        (int64_t)NumericEditCursor_GetStep(&s_edit_cursor);
    if (key == KEY_ID_STAR) delta = -delta;
    if (MassMath_Add(s_value, delta, &next) &&
        ((s_edit_kind != MENU_EDIT_MASS) ||
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
         (next >= ((s_mass_field == CONFIG_MASS_FIELD_CAPACITY) ? 1 :
                  ((s_mass_field == CONFIG_MASS_FIELD_ALARM_LOWER_LIMIT) ||
                   (s_mass_field == CONFIG_MASS_FIELD_ALARM_UPPER_LIMIT)) ?
                      -99999 : 0))) &&
#else
         (next >= ((s_mass_field == CONFIG_MASS_FIELD_CAPACITY) ? 1 : 0))) &&
#endif
        (next <= ((s_edit_kind == MENU_EDIT_MASS) ? 999999 : INT32_MAX)))
    {
        if (s_edit_kind == MENU_EDIT_STABILITY_HOLD)
        {
            if (next < 10) next = 10;
            if (next > 10000) next = 10000;
        }
        s_value = next;
    }
}

void MenuController_Init(void)
{
    s_active = false; s_editing = false; s_calibration_request = false;
    s_exit_request = false; s_factory_confirmation = false;
    s_item = MENU_ITEM_UNIT; s_advanced = false; ClearSequence();
    s_entry_ownership_allowed = false; s_save_waiting = false;
    s_exit_after_save = false;
    s_candidate_changed = false; s_brightness_previewed = false;
    s_existing_dirty_owned = false; s_existing_dirty_revision = 0U;
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    R5LocalControl_EndSession();
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    CheckweighLocalControl_EndSession();
#endif
#endif
#if defined(STAGE2A_HOST_TEST)
    s_cancel_request_count = 0U;
#endif
}

bool MenuController_Enter(void)
{
    const SystemContext *context = SystemContext_Get();
    if (s_active || (context == NULL)) return false;
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    if (!UiConfigWorkspace_Acquire(UI_CONFIG_WORKSPACE_MENU)) return false;
    if (!R5LocalControl_BeginSession())
    {
        UiConfigWorkspace_Release(UI_CONFIG_WORKSPACE_MENU);
        return false;
    }
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    if (!CheckweighLocalControl_BeginSession())
    {
        R5LocalControl_EndSession();
        UiConfigWorkspace_Release(UI_CONFIG_WORKSPACE_MENU);
        return false;
    }
#endif
#endif
    s_active = true; s_editing = false; s_factory_confirmation = false;
    s_item = MENU_ITEM_UNIT; s_advanced = false; ClearSequence();
    s_expected_revision = SystemContext_GetConfigRevision();
    if (s_existing_dirty_owned &&
             (s_existing_dirty_revision != s_expected_revision))
        s_existing_dirty_owned = false;
    s_entry_ownership_allowed =
        (SystemContext_GetConfigRevision() == SystemContext_GetSavedRevision()) ||
        (s_existing_dirty_owned &&
         (s_existing_dirty_revision == s_expected_revision));
    s_original_config = context->config;
    s_candidate_config = s_original_config;
    s_previous_page = DisplayController_GetPage();
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
    /* Calibration returns through MENU, but that page is not a RUN page. */
    if ((s_previous_page != DISPLAY_PAGE_NET) &&
        (s_previous_page != DISPLAY_PAGE_GROSS) &&
        (s_previous_page != DISPLAY_PAGE_TARE) &&
        (s_previous_page != DISPLAY_PAGE_BATTERY))
        s_previous_page = context->runtime.weight_view == WEIGHT_VIEW_GROSS ?
            DISPLAY_PAGE_GROSS : DISPLAY_PAGE_NET;
#endif
    s_candidate_changed = false;
    s_brightness_previewed = false;
    s_save_waiting = false;
    s_exit_after_save = false;
    s_last_activity_ms = BSP_TimeNowMs(); Render(); return true;
}

void MenuController_Process10ms(void)
{
    uint32_t now = BSP_TimeNowMs();
    if (!s_active) return;
    if (s_save_waiting)
    {
        PersistenceStatus status = PersistenceManager_GetStatus();
        if (((status == PERSISTENCE_STATUS_SUCCESS) ||
             (status == PERSISTENCE_STATUS_NO_CHANGE)) &&
            (SystemContext_GetSavedRevision() == s_save_revision) &&
            (SystemContext_GetConfigRevision() == s_save_revision))
        {
            s_save_waiting = false;
            s_candidate_changed = false;
            s_brightness_previewed = false;
            s_existing_dirty_owned = false;
            ShowCode(status == PERSISTENCE_STATUS_SUCCESS ?
                DISPLAY_CODE_DONE : DISPLAY_CODE_NO_CHANGE);
            if (s_exit_after_save)
                s_message_until_ms = now + UI_MESSAGE_DEFAULT_MS;
        }
        else if (status == PERSISTENCE_STATUS_REBOOT_REQUIRED)
        {
            s_exit_after_save = false;
            ShowCode(DISPLAY_CODE_SAVE_ERROR);
        }
        else if (status == PERSISTENCE_STATUS_FAILED)
        {
            s_save_waiting = false; s_exit_after_save = false;
            RestoreOriginalBrightness();
            ShowCode(DISPLAY_CODE_SAVE_ERROR);
        }
        else if ((uint32_t)(now - s_save_started_ms) >=
                 STATUS_TRANSACTION_TIMEOUT_MS)
        {
            /* Result is uncertain: retain ownership and block every key. The
               same operation may still finish, but it must never be retried. */
            s_exit_after_save = false;
            ShowCode(DISPLAY_CODE_SAVE_ERROR);
        }
        return;
    }
    if (s_exit_after_save &&
        ((int32_t)(now - s_message_until_ms) >= 0))
    {
        ExitMenu();
        return;
    }
    if (s_editing &&
        ((s_edit_kind == MENU_EDIT_MASS) ||
         (s_edit_kind == MENU_EDIT_STABILITY_HOLD)) &&
        NumericEditCursor_Process(&s_edit_cursor, now))
        Render();
    if ((s_sequence_count != 0U) &&
        (((uint32_t)(now - s_sequence_last_ms) > 1000U) ||
         ((uint32_t)(now - s_sequence_start_ms) > 4000U)))
        ReplaySequence();
    if ((uint32_t)(now - s_last_activity_ms) >= MENU_TIMEOUT_MS)
    {
        if (s_factory_confirmation) (void)MenuController_Command(
            COMMAND_FACTORY_RESET_CANCEL, 0, 0, 0U, 0);
        DiscardCandidate();
        s_factory_confirmation = false;
        ExitMenu();
    }
}

bool MenuController_HandleKeyEvent(const KeyEvent *event)
{
    const SystemContext *context;
    if (!s_active || (event == NULL) ||
        ((event->type != KEY_EVENT_SHORT) &&
         (event->type != KEY_EVENT_REPEAT) &&
         (event->type != KEY_EVENT_LONG))) return false;
    if (s_save_waiting || s_exit_after_save)
        return true;
    s_last_activity_ms = event->timestamp_ms;
    if (HandleAdvancedSequence(event)) return true;
    if (s_factory_confirmation)
    {
        if ((event->key == KEY_ID_FUNCTION) &&
            (event->type == KEY_EVENT_LONG))
        {
            (void)MenuController_Command(COMMAND_FACTORY_RESET_CONFIRM,
                                         0, 0, 0U, 0);
            s_factory_confirmation = false;
        }
        else if ((event->key == KEY_ID_TARE) &&
                 (event->type == KEY_EVENT_SHORT))
        {
            (void)MenuController_Command(COMMAND_FACTORY_RESET_CANCEL,
                                         0, 0, 0U, 0);
            s_factory_confirmation = false; Render();
        }
        return true;
    }
    if ((event->key == KEY_ID_FUNCTION) && (event->type == KEY_EVENT_LONG))
    {
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
#if (A33_ENABLE_STAGE5PA2C_PRODUCT == 0U)
        R5LocalResult r5_result;
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
        CheckweighLocalResult checkweigh_result;
#endif
#endif
        if (s_editing && (s_edit_kind == MENU_EDIT_R5_DRIFT)) return true;
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
        if (s_editing && (s_edit_kind == MENU_EDIT_CHECKWEIGH_MODE)) return true;
        if (CheckweighLocalControl_HasCandidate())
        {
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
            RequestCheckweighCandidateSave(event->timestamp_ms);
#else
            checkweigh_result = CheckweighLocalControl_Apply();
            if (checkweigh_result == CHECKWEIGH_LOCAL_OK)
            {
                ShowCode(DISPLAY_CODE_DONE);
                s_exit_after_save = true;
                s_message_until_ms = event->timestamp_ms +
                    UI_MESSAGE_DEFAULT_MS;
            }
            else ShowCode(checkweigh_result == CHECKWEIGH_LOCAL_BUSY ?
                DISPLAY_CODE_BUSY : DISPLAY_CODE_ERROR);
#endif
            return true;
        }
#endif
        if (R5LocalControl_HasCandidate())
        {
#if (A33_ENABLE_STAGE5PA2C_PRODUCT != 0U)
            RequestR5CandidateSave(event->timestamp_ms);
#else
            r5_result = R5LocalControl_Apply();
            if (r5_result == R5_LOCAL_RESULT_OK)
            {
                ShowCode(DISPLAY_CODE_DONE);
                s_exit_after_save = true;
                s_message_until_ms = event->timestamp_ms +
                    UI_MESSAGE_DEFAULT_MS;
            }
            else ShowCode(r5_result == R5_LOCAL_RESULT_BUSY ?
                DISPLAY_CODE_BUSY : DISPLAY_CODE_ERROR);
#endif
            return true;
        }
#endif
        RequestSave(event->timestamp_ms);
        return true;
    }
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    if (CheckweighLocalControl_HasCandidate())
    {
        if ((event->key == KEY_ID_TARE) &&
            (event->type == KEY_EVENT_SHORT))
        {
            CheckweighLocalControl_Cancel();
            ExitMenu();
        }
        return true;
    }
#endif
    if (R5LocalControl_HasCandidate())
    {
        if ((event->key == KEY_ID_TARE) &&
            (event->type == KEY_EVENT_SHORT))
        {
            R5LocalControl_Cancel();
            ExitMenu();
        }
        return true;
    }
#endif
    if (s_editing)
    {
        if (((event->key == KEY_ID_STAR) ||
             (event->key == KEY_ID_HASH)) &&
            ((event->type == KEY_EVENT_SHORT) ||
             (event->type == KEY_EVENT_REPEAT)))
        {
            AdjustEdit(event->key);
            if ((s_edit_kind == MENU_EDIT_MASS) ||
                (s_edit_kind == MENU_EDIT_STABILITY_HOLD))
                NumericEditCursor_ResetVisible(&s_edit_cursor,
                                               event->timestamp_ms);
        }
        else if ((event->key == KEY_ID_ZERO) &&
                 (event->type == KEY_EVENT_SHORT) &&
                 ((s_edit_kind == MENU_EDIT_MASS) ||
                  (s_edit_kind == MENU_EDIT_STABILITY_HOLD)))
            NumericEditCursor_SelectNext(&s_edit_cursor,
                                         event->timestamp_ms);
        else if ((event->key == KEY_ID_FUNCTION) &&
                 (event->type == KEY_EVENT_SHORT))
        {
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
            if (s_edit_kind == MENU_EDIT_R5_DRIFT)
            {
                R5LocalControl_Confirm();
                s_editing = false;
                Render();
                return true;
            }
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
            if (s_edit_kind == MENU_EDIT_CHECKWEIGH_MODE)
            {
                CheckweighLocalControl_Confirm();
                s_editing = false;
                Render();
                return true;
            }
#endif
#endif
            if (SystemContext_GetConfigRevision() != s_expected_revision)
            {
                CancelUnconfirmedEdit();
                ShowCode(DISPLAY_CODE_BUSY);
                return true;
            }
            if (SubmitEditValue())
            {
                s_editing = false;
                s_candidate_changed = !PersistentCodec_DeviceConfigEqual(
                    &s_candidate_config, &s_original_config);
                Render();
                return true;
            }
            s_editing = false;
            Render();
            ShowCode(DISPLAY_CODE_INVALID_CONFIG);
            return true;
        }
        else if ((event->key == KEY_ID_TARE) &&
                 (event->type == KEY_EVENT_SHORT))
        {
            CancelUnconfirmedEdit();
            Render();
            return true;
        }
        Render(); return true;
    }
    if (((event->key == KEY_ID_STAR) || (event->key == KEY_ID_HASH)) &&
        ((event->type == KEY_EVENT_SHORT) ||
         (event->type == KEY_EVENT_REPEAT)))
        Navigate(event->key);
    else if ((event->key == KEY_ID_TARE) &&
             (event->type == KEY_EVENT_SHORT))
    {
        DiscardCandidate();
        ExitMenu();
        return true;
    }
    else if ((event->key == KEY_ID_FUNCTION) &&
             (event->type == KEY_EVENT_SHORT))
    {
        context = SystemContext_Get();
        if (((s_item == MENU_ITEM_UNIT) ||
             (s_item == MENU_ITEM_PROFILE)) && (context != NULL))
        {
            if (!BeginEdit(s_item, event->timestamp_ms))
                ShowCode(s_begin_error);
            return true;
        }
        else if (s_item == MENU_ITEM_CALIBRATION)
        {
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
            /* Calibrate only from a clean snapshot. Never discard a pending
               local edit or write another owner's dirty config to Flash. */
            if (s_candidate_changed || (context == NULL) ||
                context->runtime.config_dirty ||
                (SystemContext_GetConfigRevision() !=
                 SystemContext_GetSavedRevision()))
            {
                ShowCode(DISPLAY_CODE_BUSY);
                return true;
            }
#endif
            s_calibration_request = true;
        }
        else if (s_item == MENU_ITEM_FACTORY_RESET)
        {
            if (MenuController_Command(COMMAND_FACTORY_RESET_REQUEST,
                0, 0, 0U, 0) == COMMAND_RESULT_ACCEPTED)
            {
                s_factory_confirmation = true;
                ShowCode(DISPLAY_CODE_RESET_QUERY);
            }
            return true;
        }
        else if (
#if (A33_ENABLE_STAGE5PA2C_PRODUCT == 0U)
                 (s_item == MENU_ITEM_SAMPLE_RATE) ||
#endif
                 (s_item == MENU_ITEM_GAIN) ||
                 ((s_item == MENU_ITEM_OVERLOAD) && (context != NULL) &&
                  (context->config.metrology.compliance_mode ==
                   METROLOGY_COMPLIANCE_CLASS_III_REFERENCE)))
        {
            ShowCode(DISPLAY_CODE_READ_ONLY); return true;
        }
        else if (!BeginEdit(s_item, event->timestamp_ms))
        {
            ShowCode(s_begin_error);
            return true;
        }
    }
    Render(); return true;
}

void MenuController_Cancel(void)
{
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    if (!s_active) return;
#endif
    DiscardCandidate();
    if (s_factory_confirmation) (void)MenuController_Command(
        COMMAND_FACTORY_RESET_CANCEL, 0, 0, 0U, 0);
    s_active = false; s_editing = false; s_factory_confirmation = false;
    s_advanced = false; ClearSequence();
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    R5LocalControl_EndSession();
#if (A33_ENABLE_STAGE5NB_BETA != 0U)
    CheckweighLocalControl_EndSession();
#endif
    UiConfigWorkspace_Release(UI_CONFIG_WORKSPACE_MENU);
#endif
}

bool MenuController_IsActive(void) { return s_active; }
bool MenuController_TakeCalibrationRequest(void)
{ bool value = s_calibration_request; s_calibration_request = false; return value; }
bool MenuController_TakeExitRequest(void)
{ bool value = s_exit_request; s_exit_request = false; return value; }
MenuItem MenuController_GetItem(void) { return s_item; }
bool MenuController_IsAdvanced(void) { return s_advanced; }
void MenuController_AllowCurrentDirtySave(void)
{
    const SystemContext *context = SystemContext_Get();
    if ((context != NULL) && context->runtime.config_dirty)
    {
        s_existing_dirty_owned = true;
        s_existing_dirty_revision = SystemContext_GetConfigRevision();
    }
}
#if defined(STAGE2A_HOST_TEST)
uint32_t MenuController_GetCancelRequestCount(void)
{ return s_cancel_request_count; }
bool MenuController_HasLocalPendingSave(void)
{ return s_active && s_candidate_changed; }
uint32_t MenuController_GetLocalPendingRevision(void)
{ return (s_active && s_candidate_changed) ? s_expected_revision : 0U; }
bool MenuController_GetCandidate(DeviceConfig *config)
{
    if (!s_active || (config == NULL)) return false;
    *config = s_candidate_config;
    return true;
}

#endif
