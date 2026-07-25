#include "menu_controller.h"

#include "bsp_time.h"
#include "command_service.h"
#include "config_edit.h"
#include "display_controller.h"
#include "display_codes.h"
#include "mass_math.h"
#include "metrology_manager.h"
#include "project_config.h"
#include "system_context.h"
#include "unit_converter.h"

#include <limits.h>
#include <stddef.h>

typedef enum
{
    MENU_EDIT_NONE = 0,
    MENU_EDIT_INTEGER,
    MENU_EDIT_MASS,
    MENU_EDIT_UNIT_DISPLAY,
    MENU_EDIT_FILTER,
    MENU_EDIT_STABILITY_HOLD
} MenuEditKind;

static const char s_labels[MENU_ITEM_COUNT][6] = {
    {'U','n','I','t',' ',' '}, {'P','r','O','F',' ',' '},
    {'C','A','L',' ',' ',' '}, {'C','A','P',' ',' ',' '},
    {'d','I','U',' ',' ',' '}, {'d','P',' ',' ',' ',' '},
    {'F','I','L','t',' ',' '}, {'S','t','A','b',' ',' '},
    {'Z','r','n','G',' ',' '}, {'O','L',' ',' ',' ',' '},
    {'b','r','I','G','H','t'}, {'S','P','d',' ',' ',' '},
    {'G','A','I','n',' ',' '}, {'t','r','r','E','t',' '},
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
static uint16_t s_step_multiplier;
static MassUnit s_edit_unit;
static WeighingProfileId s_edit_profile;
static UnitDisplayConfig s_edit_display;
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
    if ((s_edit_kind == MENU_EDIT_MASS) &&
        (s_value <= INT32_MAX) && (s_value >= INT32_MIN))
    {
        (void)DisplayController_SetNumericPage(DISPLAY_PAGE_EDIT,
            (int32_t)s_value, s_edit_display.decimal_places);
    }
    else
    {
        FormatInteger(s_value, text);
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_EDIT, text);
    }
}

static void ClearSequence(void)
{
    s_sequence_count = 0U;
}

static void Navigate(KeyId key)
{
    if (s_advanced)
    {
        s_item = (key == KEY_ID_HASH) ?
            (MenuItem)(((uint32_t)s_item + 1U) % MENU_ITEM_COUNT) :
            (MenuItem)(((uint32_t)s_item + MENU_ITEM_COUNT - 1U) %
                       MENU_ITEM_COUNT);
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

static bool BeginEdit(MenuItem item)
{
    const SystemContext *context = SystemContext_Get();
    const MetrologyConfig *metrology;
    const WeighingProfileConfig *profile;
    DisplayWeightValue display_value;
    MassValueUg mass = 0;
    if (context == NULL) return false;
    metrology = &context->config.metrology;
    s_edit_unit = metrology->active_unit;
    s_edit_profile = metrology->active_profile;
    s_edit_display = metrology->unit_display[s_edit_unit];
    profile = &metrology->profiles[s_edit_profile];
    s_step_multiplier = 1U;
    switch (item)
    {
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
            s_edit_kind = MENU_EDIT_INTEGER;
            s_integer_field = CONFIG_FIELD_TARE_RETENTION;
            s_value = context->config.system.tare_power_loss_retention ? 1 : 0;
            break;
        default: return false;
    }
    if (s_edit_kind == MENU_EDIT_MASS)
    {
        if (!UnitConverter_MassToDisplay(mass, s_edit_unit, &s_edit_display,
                                         &display_value) ||
            !display_value.valid || display_value.overflow)
            return false;
        s_value = display_value.display_count;
    }
    if (MenuController_Command(COMMAND_BEGIN_CONFIG_EDIT, 0, 0, 0U, 0) !=
        COMMAND_RESULT_OK) return false;
    s_editing = true;
    Render();
    return true;
}

static bool SubmitEditValue(void)
{
    MassValueUg mass;
    uint8_t strength;
    switch (s_edit_kind)
    {
        case MENU_EDIT_INTEGER:
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
        case MENU_EDIT_NONE:
        default: return false;
    }
}

static void AdjustEdit(KeyId key)
{
    int64_t delta;
    int64_t next;
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
    delta = (s_edit_kind == MENU_EDIT_MASS) ?
        (int64_t)s_edit_display.division_digit * s_step_multiplier :
        (int64_t)s_step_multiplier;
    if (key == KEY_ID_STAR) delta = -delta;
    if (MassMath_Add(s_value, delta, &next) &&
        ((s_edit_kind != MENU_EDIT_MASS) ||
         (next >= ((s_mass_field == CONFIG_MASS_FIELD_CAPACITY) ? 1 : 0))) &&
        (next <= INT32_MAX))
        s_value = next;
}

static MassUnit NextUnit(const MetrologyConfig *config)
{
    uint8_t offset;
    for (offset = 1U; offset <= MASS_UNIT_COUNT; ++offset)
    {
        MassUnit unit = (MassUnit)(((uint32_t)config->active_unit + offset) %
                                   MASS_UNIT_COUNT);
        if (((config->enabled_unit_mask & (uint8_t)(1U << unit)) != 0U) &&
            !((config->compliance_mode ==
               METROLOGY_COMPLIANCE_CLASS_III_REFERENCE) &&
              (unit == MASS_UNIT_LB))) return unit;
    }
    return config->active_unit;
}

void MenuController_Init(void)
{
    s_active = false; s_editing = false; s_calibration_request = false;
    s_exit_request = false; s_factory_confirmation = false;
    s_item = MENU_ITEM_UNIT; s_advanced = false; ClearSequence();
}

bool MenuController_Enter(void)
{
    if (s_active) return false;
    s_active = true; s_editing = false; s_factory_confirmation = false;
    s_item = MENU_ITEM_UNIT; s_advanced = false; ClearSequence();
    s_last_activity_ms = BSP_TimeNowMs(); Render(); return true;
}

void MenuController_Process10ms(void)
{
    uint32_t now = BSP_TimeNowMs();
    if (!s_active) return;
    if ((s_sequence_count != 0U) &&
        (((uint32_t)(now - s_sequence_last_ms) > 1000U) ||
         ((uint32_t)(now - s_sequence_start_ms) > 4000U)))
        ReplaySequence();
    if ((uint32_t)(now - s_last_activity_ms) >= MENU_TIMEOUT_MS)
    {
        if (s_editing) (void)MenuController_Command(
            COMMAND_CANCEL_CONFIG_EDIT, 0, 0, 0U, 0);
        if (s_factory_confirmation) (void)MenuController_Command(
            COMMAND_FACTORY_RESET_CANCEL, 0, 0, 0U, 0);
        s_active = false; s_editing = false; s_factory_confirmation = false;
        ClearSequence(); s_exit_request = true;
    }
}

bool MenuController_HandleKeyEvent(const KeyEvent *event)
{
    const SystemContext *context;
    if (!s_active || (event == NULL) ||
        ((event->type != KEY_EVENT_SHORT) &&
         (event->type != KEY_EVENT_REPEAT) &&
         (event->type != KEY_EVENT_LONG))) return false;
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
        if (s_editing) (void)MenuController_Command(
            COMMAND_CANCEL_CONFIG_EDIT, 0, 0, 0U, 0);
        s_editing = false; s_active = false; ClearSequence();
        s_exit_request = true; return true;
    }
    if (s_editing)
    {
        if ((event->key == KEY_ID_STAR) || (event->key == KEY_ID_HASH))
            AdjustEdit(event->key);
        else if ((event->key == KEY_ID_ZERO) &&
                 (event->type == KEY_EVENT_SHORT))
            s_step_multiplier = (s_step_multiplier >= 1000U) ? 1U :
                (uint16_t)(s_step_multiplier * 10U);
        else if ((event->key == KEY_ID_FUNCTION) &&
                 (event->type == KEY_EVENT_SHORT))
        {
            if (SubmitEditValue() &&
                (MenuController_Command(COMMAND_COMMIT_CONFIG_EDIT,
                    0, 0, 0U, 0) == COMMAND_RESULT_OK))
            {
                s_editing = false; Render(); ShowCode(DISPLAY_CODE_RAM_SAVE);
                return true;
            }
            ShowCode(DISPLAY_CODE_INVALID_CONFIG);
            return true;
        }
        else if ((event->key == KEY_ID_TARE) &&
                 (event->type == KEY_EVENT_SHORT))
        {
            (void)MenuController_Command(COMMAND_CANCEL_CONFIG_EDIT,
                                         0, 0, 0U, 0);
            s_editing = false;
        }
        Render(); return true;
    }
    if ((event->key == KEY_ID_STAR) || (event->key == KEY_ID_HASH))
        Navigate(event->key);
    else if ((event->key == KEY_ID_TARE) &&
             (event->type == KEY_EVENT_SHORT))
    {
        s_active = false; s_exit_request = true;
    }
    else if ((event->key == KEY_ID_FUNCTION) &&
             (event->type == KEY_EVENT_SHORT))
    {
        context = SystemContext_Get();
        if ((s_item == MENU_ITEM_UNIT) && (context != NULL))
        {
            MassUnit unit = NextUnit(&context->config.metrology);
            if (MenuController_Command(COMMAND_SET_DISPLAY_UNIT, unit, 0,
                                       0U, 0) == COMMAND_RESULT_OK)
            {
                static const char hints[MASS_UNIT_COUNT][6] = {
                    {' ', ' ', ' ', ' ', 'k', 'g'},
                    {' ', ' ', ' ', ' ', ' ', 'g'},
                    {' ', ' ', ' ', ' ', 'l', 'b'}};
                DisplayController_ShowMessage(hints[unit], 500U);
            }
            else ShowCode(DISPLAY_CODE_UNIT_ERROR);
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
            return true;
        }
        else if (s_item == MENU_ITEM_CALIBRATION)
        {
            s_active = false; s_calibration_request = true;
        }
        else if (s_item == MENU_ITEM_EXIT)
        {
            s_active = false; s_exit_request = true;
        }
        else if (s_item == MENU_ITEM_SAVE)
        {
            CommandResult result = MenuController_Command(
                COMMAND_REQUEST_CONFIG_SAVE, 0, 0, 0U, 0);
            if ((result != COMMAND_RESULT_ACCEPTED) &&
                (result != COMMAND_RESULT_OK)) ShowCode(DISPLAY_CODE_SAVE_ERROR);
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
        else if (!BeginEdit(s_item))
        {
            ShowCode(DISPLAY_CODE_BUSY);
            return true;
        }
    }
    Render(); return true;
}

void MenuController_Cancel(void)
{
    if (s_editing) (void)MenuController_Command(COMMAND_CANCEL_CONFIG_EDIT,
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
