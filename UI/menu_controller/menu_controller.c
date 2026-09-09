#include "menu_controller.h"

#include "bsp_time.h"
#include "command_service.h"
#include "config_edit.h"
#include "display_controller.h"
#include "display_codes.h"
#include "mass_math.h"
#include "metrology_manager.h"
#include "persistence_manager.h"
#include "numeric_edit_cursor.h"
#include "project_config.h"
#include "system_context.h"
#include "unit_converter.h"
#include "weighing_profile_manager.h"

#include <limits.h>
#include <stddef.h>

typedef enum
{
    MENU_EDIT_NONE = 0,
    MENU_EDIT_UNIT,
    MENU_EDIT_INTEGER,
    MENU_EDIT_MASS,
    MENU_EDIT_UNIT_DISPLAY,
    MENU_EDIT_FILTER,
    MENU_EDIT_STABILITY_HOLD,
    MENU_EDIT_BOOL
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
    ,
    MENU_EDIT_ALARM_SOURCE
#endif
} MenuEditKind;

static const char s_labels[MENU_ITEM_COUNT][6] = {
    {'U','n','I','t',' ',' '}, {'P','r','O','F',' ',' '},
    {'C','A','L',' ',' ',' '}, {'C','A','P',' ',' ',' '},
    {'d','I','U',' ',' ',' '}, {'d','P',' ',' ',' ',' '},
    {'F','I','L','t',' ',' '}, {'S','t','A','b',' ',' '},
    {'Z','r','n','G',' ',' '}, {'P','-','Z','r',' ',' '},
    {'O','L',' ',' ',' ',' '},
    {'b','r','I','G','H','t'}, {'S','P','d',' ',' ',' '},
    {'G','A','I','n',' ',' '}, {'t','r','r','E','t',' '},
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
    MENU_ITEM_TARE_RETENTION, MENU_ITEM_SAVE, MENU_ITEM_EXIT};

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
static bool s_local_pending_save;
static bool s_entry_ownership_allowed;
static uint32_t s_local_pending_revision;
static bool s_save_waiting;
static bool s_exit_after_save;
static bool s_profile_apply_pending;
static WeighingProfileId s_profile_target;
static uint32_t s_profile_start_revision;
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
    s_active = false;
    s_editing = false;
    s_save_waiting = false;
    s_factory_confirmation = false;
    ClearSequence();
    s_exit_request = true;
}

static void CancelUnconfirmedEdit(void)
{
    if (s_editing && (s_edit_kind != MENU_EDIT_UNIT))
    {
        (void)MenuController_Command(COMMAND_CANCEL_CONFIG_EDIT,
            0, 0, 0U, 0);
#if defined(STAGE2A_HOST_TEST)
        ++s_cancel_request_count;
#endif
    }
    s_editing = false;
}

static void RefreshLocalOwnership(void)
{
    uint32_t current = SystemContext_GetConfigRevision();
    uint32_t saved = SystemContext_GetSavedRevision();
    if (current == saved)
    {
        s_local_pending_save = false;
        s_local_pending_revision = 0U;
    }
    else if (s_local_pending_save && (current != s_local_pending_revision))
    {
        /* ConfigStore saves the whole snapshot. A foreign revision permanently
           invalidates the older menu ownership; no field merge is attempted. */
        s_local_pending_save = false;
        s_local_pending_revision = 0U;
        s_entry_ownership_allowed = false;
    }
}

static void RecordLocalConfirmation(void)
{
    uint32_t current = SystemContext_GetConfigRevision();
    if (s_entry_ownership_allowed)
    {
        s_local_pending_save = true;
        s_local_pending_revision = current;
    }
    s_expected_revision = current;
}

static void RequestSave(bool exit_after, bool explicit_save, uint32_t now_ms)
{
    CommandResult result;
    CancelUnconfirmedEdit();
    if (s_profile_apply_pending)
    {
        ShowCode(DISPLAY_CODE_BUSY);
        return;
    }
    RefreshLocalOwnership();
    if (SystemContext_GetConfigRevision() == SystemContext_GetSavedRevision())
    {
        ShowCode(DISPLAY_CODE_NO_CHANGE);
        if (exit_after)
        {
            s_exit_after_save = true;
            s_message_until_ms = now_ms + UI_MESSAGE_DEFAULT_MS;
        }
        return;
    }
    if (!explicit_save && !s_local_pending_save)
    {
        ShowCode(DISPLAY_CODE_BUSY);
        return;
    }
    /* ConfigStore persists the whole snapshot, so any foreign revision makes
       an automatic menu save unsafe; no field-level merge is attempted. */
    if ((SystemContext_GetConfigRevision() != s_expected_revision) ||
        (!explicit_save &&
         (SystemContext_GetConfigRevision() != s_local_pending_revision)))
    {
        if (!explicit_save)
        {
            s_local_pending_save = false;
            s_local_pending_revision = 0U;
        }
        ShowCode(DISPLAY_CODE_BUSY);
        return;
    }
    result = PersistenceManager_RequestSave();
    if ((result != COMMAND_RESULT_ACCEPTED) &&
        (result != COMMAND_RESULT_OK))
    {
        ShowCode(DISPLAY_CODE_SAVE_ERROR);
        return;
    }
    s_save_revision = SystemContext_GetConfigRevision();
    s_save_started_ms = now_ms;
    s_save_waiting = true;
    s_exit_after_save = exit_after;
    ShowCode(DISPLAY_CODE_SAVE);
}

static void Navigate(KeyId key)
{
    if (s_advanced)
    {
        s_item = (key == KEY_ID_HASH) ?
            (MenuItem)(((uint32_t)s_item + 1U) % MENU_ITEM_COUNT) :
            (MenuItem)(((uint32_t)s_item + MENU_ITEM_COUNT - 1U) %
                       MENU_ITEM_COUNT);
        while ((s_item == MENU_ITEM_SAMPLE_RATE) ||
               (s_item == MENU_ITEM_GAIN))
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
    metrology = &context->config.metrology;
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
            s_value = context->config.system.startup_auto_zero_enable ? 1 : 0;
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
        case MENU_ITEM_STABILITY:
            s_edit_kind = MENU_EDIT_STABILITY_HOLD;
            s_value = profile->stability_hold_ms;
            break;
        case MENU_ITEM_BRIGHTNESS:
            s_edit_kind = MENU_EDIT_INTEGER;
            s_integer_field = CONFIG_FIELD_DISPLAY_BRIGHTNESS;
            s_value = context->config.display.brightness;
            break;
        case MENU_ITEM_TARE_RETENTION:
            s_edit_kind = MENU_EDIT_BOOL;
            s_integer_field = CONFIG_FIELD_TARE_RETENTION;
            s_value = context->config.system.tare_power_loss_retention ? 1 : 0;
            break;
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
        case MENU_ITEM_LIMIT_ENABLE:
            s_edit_kind = MENU_EDIT_BOOL;
            s_integer_field = CONFIG_FIELD_LIMIT_ENABLE;
            s_value = context->config.alarm.limit_function_enable ? 1 : 0;
            break;
        case MENU_ITEM_ALARM_LOWER_LIMIT:
            s_edit_kind = MENU_EDIT_MASS;
            s_mass_field = CONFIG_MASS_FIELD_ALARM_LOWER_LIMIT;
            mass = context->config.alarm.lower_limit_ug;
            break;
        case MENU_ITEM_ALARM_UPPER_LIMIT:
            s_edit_kind = MENU_EDIT_MASS;
            s_mass_field = CONFIG_MASS_FIELD_ALARM_UPPER_LIMIT;
            mass = context->config.alarm.upper_limit_ug;
            break;
        case MENU_ITEM_ALARM_HYSTERESIS:
            s_edit_kind = MENU_EDIT_MASS;
            s_mass_field = CONFIG_MASS_FIELD_ALARM_HYSTERESIS;
            mass = context->config.alarm.hysteresis_ug;
            break;
        case MENU_ITEM_ALARM_SOURCE:
            s_edit_kind = MENU_EDIT_ALARM_SOURCE;
            s_integer_field = CONFIG_FIELD_ALARM_WEIGHT_SOURCE;
            s_value = context->config.alarm.weight_source;
            break;
        case MENU_ITEM_INTERNAL_BUZZER:
            s_edit_kind = MENU_EDIT_BOOL;
            s_integer_field = CONFIG_FIELD_INTERNAL_BUZZER_ENABLE;
            s_value = context->config.alarm.internal_buzzer_enable ? 1 : 0;
            break;
        case MENU_ITEM_EXTERNAL_BUZZER:
            s_edit_kind = MENU_EDIT_BOOL;
            s_integer_field = CONFIG_FIELD_EXTERNAL_BUZZER_ENABLE;
            s_value = context->config.alarm.external_buzzer_enable ? 1 : 0;
            break;
        case MENU_ITEM_QUALIFIED_BEEP:
            s_edit_kind = MENU_EDIT_BOOL;
            s_integer_field = CONFIG_FIELD_QUALIFIED_BEEP_ENABLE;
            s_value = context->config.alarm.qualified_beep_enable ? 1 : 0;
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
    if (MenuController_Command(COMMAND_BEGIN_CONFIG_EDIT, 0, 0, 0U, 0) !=
        COMMAND_RESULT_OK) return false;
    s_editing = true;
    Render();
    if (s_begin_warning) ShowCode(DISPLAY_CODE_UNIT_RANGE);
    return true;
}

static bool SubmitEditValue(void)
{
    MassValueUg mass;
    uint8_t strength;
    switch (s_edit_kind)
    {
        case MENU_EDIT_INTEGER:
        case MENU_EDIT_BOOL:
#if (ENABLE_STAGE5E_A3_LOCAL_MENU != 0U)
        case MENU_EDIT_ALARM_SOURCE:
#endif
            return MenuController_Command(COMMAND_SET_CONFIG_FIELD,
                s_integer_field, (int32_t)s_value, 0U, 0) == COMMAND_RESULT_OK;
        case MENU_EDIT_MASS:
            return UnitConverter_CountToMass(s_value, s_edit_unit,
                s_edit_display.decimal_places, &mass) &&
                (MenuController_Command(COMMAND_SET_CONFIG_MASS_FIELD,
                    s_mass_field, 0, 0U, mass) == COMMAND_RESULT_OK);
        case MENU_EDIT_UNIT_DISPLAY:
            return MenuController_Command(COMMAND_SET_UNIT_DISPLAY_CONFIG,
                s_edit_unit, s_edit_display.decimal_places,
                s_edit_display.division_digit, 0) == COMMAND_RESULT_OK;
        case MENU_EDIT_FILTER:
            strength = (s_value == FILTER_MODE_NONE) ? 0U :
                       (s_value == FILTER_MODE_AVERAGE) ? 2U : 1U;
            return (MenuController_Command(COMMAND_SET_PROFILE_FIELD,
                    s_edit_profile, CONFIG_PROFILE_FIELD_FILTER_MODE, 0U,
                    s_value) == COMMAND_RESULT_OK) &&
                   (MenuController_Command(COMMAND_SET_PROFILE_FIELD,
                    s_edit_profile, CONFIG_PROFILE_FIELD_FILTER_STRENGTH, 0U,
                    strength) == COMMAND_RESULT_OK);
        case MENU_EDIT_STABILITY_HOLD:
            return MenuController_Command(COMMAND_SET_PROFILE_FIELD,
                s_edit_profile, CONFIG_PROFILE_FIELD_STABILITY_HOLD_MS, 0U,
                s_value) == COMMAND_RESULT_OK;
        case MENU_EDIT_UNIT:
        case MENU_EDIT_NONE:
        default: return false;
    }
}

static void AdjustEdit(KeyId key)
{
    int64_t delta;
    int64_t next;
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
            if (((context->config.metrology.enabled_unit_mask &
                  (uint8_t)(1U << candidate)) != 0U) &&
                !((context->config.metrology.compliance_mode ==
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
    if (s_item == MENU_ITEM_BRIGHTNESS)
    {
        s_value = (key == KEY_ID_HASH) ?
            ((s_value >= 7) ? 1 : s_value + 1) :
            ((s_value <= 1) ? 7 : s_value - 1);
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
    s_local_pending_save = false; s_local_pending_revision = 0U;
    s_entry_ownership_allowed = false; s_save_waiting = false;
    s_exit_after_save = false; s_profile_apply_pending = false;
#if defined(STAGE2A_HOST_TEST)
    s_cancel_request_count = 0U;
#endif
}

bool MenuController_Enter(void)
{
    if (s_active) return false;
    s_active = true; s_editing = false; s_factory_confirmation = false;
    s_item = MENU_ITEM_UNIT; s_advanced = false; ClearSequence();
    s_expected_revision = SystemContext_GetConfigRevision();
    RefreshLocalOwnership();
    s_entry_ownership_allowed =
        (SystemContext_GetConfigRevision() == SystemContext_GetSavedRevision()) ||
        (s_local_pending_save &&
         (SystemContext_GetConfigRevision() == s_local_pending_revision));
    s_save_waiting = false;
    s_exit_after_save = false; s_profile_apply_pending = false;
    s_last_activity_ms = BSP_TimeNowMs(); Render(); return true;
}

void MenuController_Process10ms(void)
{
    uint32_t now = BSP_TimeNowMs();
    if (!s_active) return;
    if (s_profile_apply_pending)
    {
        if (!WeighingProfileManager_IsBusy())
        {
            const SystemContext *context = SystemContext_Get();
            s_profile_apply_pending = false;
            if ((WeighingProfileManager_GetLastResult() == COMMAND_RESULT_OK) &&
                (context != NULL) &&
                (SystemContext_GetConfigRevision() !=
                 s_profile_start_revision) &&
                (WeighingProfileManager_GetResultRevision() ==
                 SystemContext_GetConfigRevision()) &&
                (context->config.metrology.active_profile == s_profile_target))
            {
                RecordLocalConfirmation();
                ShowCode(DISPLAY_CODE_RAM_SAVE);
            }
            else
            {
                RefreshLocalOwnership();
                ShowCode(DISPLAY_CODE_ERROR);
            }
        }
        return;
    }
    RefreshLocalOwnership();
    if (s_save_waiting)
    {
        PersistenceStatus status = PersistenceManager_GetStatus();
        if (((status == PERSISTENCE_STATUS_SUCCESS) ||
             (status == PERSISTENCE_STATUS_NO_CHANGE)) &&
            (SystemContext_GetSavedRevision() == s_save_revision) &&
            (SystemContext_GetConfigRevision() == s_save_revision))
        {
            s_save_waiting = false;
            s_local_pending_save = false;
            s_local_pending_revision = 0U;
            ShowCode(status == PERSISTENCE_STATUS_SUCCESS ?
                DISPLAY_CODE_DONE : DISPLAY_CODE_NO_CHANGE);
            if (s_exit_after_save)
                s_message_until_ms = now + UI_MESSAGE_DEFAULT_MS;
        }
        else if ((status == PERSISTENCE_STATUS_FAILED) ||
                 (status == PERSISTENCE_STATUS_REBOOT_REQUIRED))
        {
            s_save_waiting = false; s_exit_after_save = false;
            ShowCode(DISPLAY_CODE_SAVE_ERROR);
        }
        else if ((uint32_t)(now - s_save_started_ms) >=
                 STATUS_TRANSACTION_TIMEOUT_MS)
        {
            /* The result is uncertain. Keep valid local ownership, but require
               another explicit user action before issuing any further SAVE. */
            s_save_waiting = false; s_exit_after_save = false;
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
        CancelUnconfirmedEdit();
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
    if (s_save_waiting || s_exit_after_save || s_profile_apply_pending)
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
        RequestSave(true, false, event->timestamp_ms);
        return true;
    }
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
            if (SystemContext_GetConfigRevision() != s_expected_revision)
            {
                CancelUnconfirmedEdit();
                ShowCode(DISPLAY_CODE_BUSY);
                return true;
            }
            if (s_edit_kind == MENU_EDIT_UNIT)
            {
                if (s_candidate_unit == s_original_unit)
                {
                    s_editing = false;
                    Render();
                    return true;
                }
                if (MenuController_Command(COMMAND_SET_DISPLAY_UNIT,
                        s_candidate_unit, 0, 0U, 0) == COMMAND_RESULT_OK)
                {
                    s_editing = false;
                    Render();
                    ShowCode(DISPLAY_CODE_RAM_SAVE);
                    RecordLocalConfirmation();
                }
                else ShowCode(DISPLAY_CODE_UNIT_ERROR);
                return true;
            }
            if (SubmitEditValue() &&
                (MenuController_Command(COMMAND_COMMIT_CONFIG_EDIT,
                    0, 0, 0U, 0) == COMMAND_RESULT_OK))
            {
                s_editing = false; Render(); ShowCode(DISPLAY_CODE_RAM_SAVE);
                RecordLocalConfirmation();
                return true;
            }
            (void)MenuController_Command(COMMAND_CANCEL_CONFIG_EDIT,
                                         0, 0, 0U, 0);
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
        ExitMenu();
    }
    else if ((event->key == KEY_ID_FUNCTION) &&
             (event->type == KEY_EVENT_SHORT))
    {
        context = SystemContext_Get();
        if ((s_item == MENU_ITEM_UNIT) && (context != NULL))
        {
            if (!BeginEdit(s_item, event->timestamp_ms))
                ShowCode(s_begin_error);
            return true;
        }
        else if ((s_item == MENU_ITEM_PROFILE) && (context != NULL))
        {
            CommandResult result = MenuController_Command(
                COMMAND_SWITCH_WEIGHING_PROFILE,
                (context->config.metrology.active_profile ==
                 WEIGHING_PROFILE_HIGH_PRECISION) ?
                 WEIGHING_PROFILE_HIGH_SPEED : WEIGHING_PROFILE_HIGH_PRECISION,
                0, 0U, 0);
            ShowCode((result == COMMAND_RESULT_ACCEPTED) ?
                DISPLAY_CODE_APPLYING : DISPLAY_CODE_BUSY);
            if (result == COMMAND_RESULT_ACCEPTED)
            {
                s_profile_target =
                    (context->config.metrology.active_profile ==
                     WEIGHING_PROFILE_HIGH_PRECISION) ?
                    WEIGHING_PROFILE_HIGH_SPEED :
                    WEIGHING_PROFILE_HIGH_PRECISION;
                s_profile_start_revision = SystemContext_GetConfigRevision();
                s_profile_apply_pending = true;
            }
            return true;
        }
        else if (s_item == MENU_ITEM_CALIBRATION)
        {
            s_calibration_request = true;
        }
        else if (s_item == MENU_ITEM_EXIT)
        {
            s_active = false; s_exit_request = true;
        }
        else if (s_item == MENU_ITEM_SAVE)
        {
            RequestSave(false, true, event->timestamp_ms);
            return true;
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
        else if ((s_item == MENU_ITEM_SAMPLE_RATE) ||
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
    if (s_editing && (s_edit_kind != MENU_EDIT_UNIT))
        (void)MenuController_Command(COMMAND_CANCEL_CONFIG_EDIT,
            0, 0, 0U, 0);
    if (s_factory_confirmation) (void)MenuController_Command(
        COMMAND_FACTORY_RESET_CANCEL, 0, 0, 0U, 0);
    s_active = false; s_editing = false; s_factory_confirmation = false;
    s_advanced = false; ClearSequence();
}

bool MenuController_IsActive(void) { return s_active; }
bool MenuController_TakeCalibrationRequest(void)
{ bool value = s_calibration_request; s_calibration_request = false; return value; }
bool MenuController_TakeExitRequest(void)
{ bool value = s_exit_request; s_exit_request = false; return value; }
MenuItem MenuController_GetItem(void) { return s_item; }
bool MenuController_IsAdvanced(void) { return s_advanced; }
#if defined(STAGE2A_HOST_TEST)
uint32_t MenuController_GetCancelRequestCount(void)
{ return s_cancel_request_count; }
bool MenuController_HasLocalPendingSave(void)
{ RefreshLocalOwnership(); return s_local_pending_save; }
uint32_t MenuController_GetLocalPendingRevision(void)
{ return s_local_pending_revision; }
#endif
