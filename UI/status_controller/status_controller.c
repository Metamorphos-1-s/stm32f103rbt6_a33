#include "status_controller.h"

#include "battery_adc.h"
#include "bsp_time.h"
#include "communication_manager.h"
#include "display_controller.h"
#include "modbus_register_map.h"
#include "persistence_manager.h"
#include "project_config.h"
#include "system_context.h"

#include <stddef.h>
#include <string.h>

#define STATUS_LABEL_MS 600U

typedef enum
{
    STATUS_MODE_BROWSE = 0,
    STATUS_MODE_EDIT,
    STATUS_MODE_APPLYING,
    STATUS_MODE_SAVING,
    STATUS_MODE_COMPLETE
} StatusMode;

static const char s_labels[STATUS_ITEM_COUNT][6] = {
    {'F','I','r',' ',' ',' '}, {'r','A','P',' ',' ',' '},
    {'S','C','H',' ',' ',' '}, {'P','r','O','F',' ',' '},
    {'S','P','d',' ',' ',' '}, {'G','A','I','n',' ',' '},
    {'b','A','t',' ',' ',' '}, {'P','r','O','t',' ',' '},
    {'A','d','d','r',' ',' '}, {'b','A','U','d',' ',' '},
    {'P','A','r',' ',' ',' '}, {'S','t','O','P',' ',' '},
    {'O','r','d','E','r',' '}
};
static const uint32_t s_baud[] = {9600U,19200U,38400U,57600U,115200U};
static bool s_active;
static bool s_confirmed;
static StatusItem s_item;
static StatusMode s_mode;
static DisplayPage s_previous_page;
static CommunicationConfig s_original;
static CommunicationConfig s_candidate;
static CommunicationConfig s_edit;
static uint32_t s_original_revision;
static uint32_t s_target_revision;
static uint32_t s_last_activity_ms;
static uint32_t s_label_until_ms;
static uint32_t s_complete_until_ms;

static uint32_t NextRevision(uint32_t revision)
{
    uint32_t next = revision + 1U;
    return next == 0xFFFFFFFFUL ? 0U : next;
}

static bool IsEditable(StatusItem item)
{
    return item >= STATUS_ITEM_ADDRESS;
}

static void Show(const char text[6])
{
    (void)DisplayController_SetTextPage(DISPLAY_PAGE_STATUS, text);
}

static void ShowNumber(uint32_t value)
{
    if (value > 999999U) value = 999999U;
    (void)DisplayController_SetNumericPage(DISPLAY_PAGE_STATUS,
                                           (int32_t)value, 0U);
}

static void RenderValue(void)
{
    const SystemContext *context = SystemContext_Get();
    const BatteryAdcState *battery = BatteryAdc_GetState();
    const WeighingProfileConfig *profile;
    if (context == NULL) return;
    profile = &context->config.metrology.profiles[
        context->config.metrology.active_profile];
    switch (s_item)
    {
        case STATUS_ITEM_FIRMWARE:
            (void)DisplayController_SetNumericPage(DISPLAY_PAGE_STATUS,
                (int32_t)(FW_RELEASE_VERSION_MAJOR * 100U +
                          FW_RELEASE_VERSION_MINOR), 2U);
            break;
        case STATUS_ITEM_MAP:
            (void)DisplayController_SetNumericPage(DISPLAY_PAGE_STATUS,
                (int32_t)(((MODBUS_REGISTER_MAP_VERSION >> 8U) & 0xFFU) *
                          100U +
                          (MODBUS_REGISTER_MAP_VERSION & 0xFFU)), 2U);
            break;
        case STATUS_ITEM_SCHEMA: ShowNumber(DEVICE_CONFIG_SCHEMA_VERSION); break;
        case STATUS_ITEM_PROFILE:
            ShowNumber(context->config.metrology.active_profile); break;
        case STATUS_ITEM_SAMPLE_RATE:
            ShowNumber(profile->sample_rate == DEVICE_CS1237_DATA_RATE_10_HZ ?
                10U : profile->sample_rate == DEVICE_CS1237_DATA_RATE_40_HZ ?
                40U : profile->sample_rate == DEVICE_CS1237_DATA_RATE_640_HZ ?
                640U : 1280U); break;
        case STATUS_ITEM_GAIN:
            ShowNumber(profile->gain == DEVICE_CS1237_GAIN_1 ? 1U :
                profile->gain == DEVICE_CS1237_GAIN_2 ? 2U :
                profile->gain == DEVICE_CS1237_GAIN_64 ? 64U : 128U); break;
        case STATUS_ITEM_BATTERY:
            if ((battery != NULL) && battery->valid)
                (void)DisplayController_SetNumericPage(DISPLAY_PAGE_STATUS,
                    (int32_t)battery->battery_mv, 3U);
            else Show("------");
            break;
        case STATUS_ITEM_PROTOCOL:
            Show(context->config.communication.protocol_mode ==
                 PROTOCOL_MODE_MODBUS_RTU ? "   rtU" : "  CUSt");
            break;
        case STATUS_ITEM_ADDRESS: ShowNumber((s_mode == STATUS_MODE_EDIT) ?
            s_edit.modbus_address : s_candidate.modbus_address); break;
        case STATUS_ITEM_BAUD: ShowNumber((s_mode == STATUS_MODE_EDIT) ?
            s_edit.baud_rate : s_candidate.baud_rate); break;
        case STATUS_ITEM_PARITY:
        {
            CommunicationParity parity = (s_mode == STATUS_MODE_EDIT) ?
                s_edit.parity : s_candidate.parity;
            Show(parity == COMM_PARITY_NONE ? "  nonE" :
                 parity == COMM_PARITY_EVEN ? "  EUEn" : "   Odd");
            break;
        }
        case STATUS_ITEM_STOP_BITS: ShowNumber(((s_mode == STATUS_MODE_EDIT) ?
            s_edit.stop_bits : s_candidate.stop_bits) == COMM_STOP_BITS_2 ?
            2U : 1U); break;
        case STATUS_ITEM_WORD_ORDER:
            Show(((s_mode == STATUS_MODE_EDIT) ? s_edit.word_order :
                s_candidate.word_order) == MODBUS_WORD_ORDER_HIGH_WORD_FIRST ?
                "    HI" : "    Lo"); break;
        case STATUS_ITEM_COUNT: default: break;
    }
}

static void ShowItem(uint32_t now_ms)
{
    Show(s_labels[s_item]);
    s_label_until_ms = now_ms + STATUS_LABEL_MS;
}

static void ExitDiscard(void)
{
    s_active = false;
    s_mode = STATUS_MODE_BROWSE;
    DisplayController_SetPage(s_previous_page);
}

static void Adjust(KeyId key)
{
    bool up = key == KEY_ID_HASH;
    if (s_item == STATUS_ITEM_ADDRESS)
        s_edit.modbus_address = up ?
            (s_edit.modbus_address >= 247U ? 1U :
             (uint8_t)(s_edit.modbus_address + 1U)) :
            (s_edit.modbus_address <= 1U ? 247U :
             (uint8_t)(s_edit.modbus_address - 1U));
    else if (s_item == STATUS_ITEM_BAUD)
    {
        uint8_t index;
        for (index = 0U; index < 5U; ++index)
            if (s_baud[index] == s_edit.baud_rate) break;
        if (index >= 5U) index = 4U;
        index = up ? (uint8_t)((index + 1U) % 5U) :
                     (uint8_t)((index + 4U) % 5U);
        s_edit.baud_rate = s_baud[index];
    }
    else if (s_item == STATUS_ITEM_PARITY)
        s_edit.parity = (CommunicationParity)((s_edit.parity +
            (up ? 1U : COMM_PARITY_COUNT - 1U)) % COMM_PARITY_COUNT);
    else if (s_item == STATUS_ITEM_STOP_BITS)
        s_edit.stop_bits = s_edit.stop_bits == COMM_STOP_BITS_1 ?
            COMM_STOP_BITS_2 : COMM_STOP_BITS_1;
    else if (s_item == STATUS_ITEM_WORD_ORDER)
        s_edit.word_order = s_edit.word_order ==
            MODBUS_WORD_ORDER_HIGH_WORD_FIRST ?
            MODBUS_WORD_ORDER_LOW_WORD_FIRST :
            MODBUS_WORD_ORDER_HIGH_WORD_FIRST;
}

static void BeginSave(uint32_t now_ms)
{
    if (s_mode == STATUS_MODE_EDIT) s_mode = STATUS_MODE_BROWSE;
    if (!s_confirmed ||
        (memcmp(&s_candidate, &s_original, sizeof(s_candidate)) == 0))
    {
        Show("noCHG ");
        s_mode = STATUS_MODE_COMPLETE;
        s_complete_until_ms = now_ms + UI_MESSAGE_DEFAULT_MS;
        return;
    }
    if (!CommunicationManager_IsConfigValid(&s_candidate))
    {
        Show("InUALd");
        return;
    }
    if (SystemContext_GetConfigRevision() != s_original_revision)
    {
        Show(" bUSY ");
        return;
    }
    if (CommunicationManager_RequestLocalApply(&s_candidate) !=
        COMMAND_RESULT_ACCEPTED)
    {
        Show(" Err  ");
        return;
    }
    Show("APPLY ");
    s_mode = STATUS_MODE_APPLYING;
}

void StatusController_Init(void)
{
    s_active = false;
    s_mode = STATUS_MODE_BROWSE;
}

bool StatusController_Enter(void)
{
    const SystemContext *context = SystemContext_Get();
    if (s_active || (context == NULL)) return false;
    s_active = true;
    s_confirmed = false;
    s_item = STATUS_ITEM_FIRMWARE;
    s_mode = STATUS_MODE_BROWSE;
    s_previous_page = DisplayController_GetPage();
    s_original = context->config.communication;
    s_candidate = s_original;
    s_original_revision = SystemContext_GetConfigRevision();
    s_last_activity_ms = BSP_TimeNowMs();
    ShowItem(s_last_activity_ms);
    return true;
}

void StatusController_Cancel(void)
{
    if (s_active) ExitDiscard();
}

void StatusController_Process10ms(void)
{
    uint32_t now = BSP_TimeNowMs();
    if (!s_active) return;
    if ((s_mode == STATUS_MODE_BROWSE) || (s_mode == STATUS_MODE_EDIT))
    {
        if ((uint32_t)(now - s_last_activity_ms) >= MENU_TIMEOUT_MS)
        {
            ExitDiscard();
            return;
        }
        if ((s_mode == STATUS_MODE_BROWSE) &&
            ((int32_t)(now - s_label_until_ms) >= 0)) RenderValue();
    }
    else if (s_mode == STATUS_MODE_APPLYING)
    {
        CommunicationApplyResult result = CommunicationManager_GetApplyResult();
        if (result == COMM_APPLY_RESULT_SUCCESS)
        {
            s_target_revision = SystemContext_GetConfigRevision();
            if ((s_target_revision != NextRevision(s_original_revision)) ||
                (memcmp(&SystemContext_Get()->config.communication,
                        &s_candidate, sizeof(s_candidate)) != 0))
            {
                Show(" bUSY ");
                s_mode = STATUS_MODE_BROWSE;
                return;
            }
            if (PersistenceManager_RequestSave() == COMMAND_RESULT_ACCEPTED)
            {
                Show(" SAUE ");
                s_mode = STATUS_MODE_SAVING;
            }
            else
            {
                Show("ErrSAU");
                s_mode = STATUS_MODE_BROWSE;
            }
        }
        else if (result == COMM_APPLY_RESULT_FAILED)
        {
            Show(" Err  ");
            s_mode = STATUS_MODE_BROWSE;
        }
    }
    else if (s_mode == STATUS_MODE_SAVING)
    {
        PersistenceStatus status = PersistenceManager_GetStatus();
        if ((status == PERSISTENCE_STATUS_SUCCESS) &&
            (SystemContext_GetSavedRevision() == s_target_revision))
        {
            Show("  donE");
            s_mode = STATUS_MODE_COMPLETE;
            s_complete_until_ms = now + UI_MESSAGE_DEFAULT_MS;
        }
        else if ((status == PERSISTENCE_STATUS_FAILED) ||
                 (status == PERSISTENCE_STATUS_REBOOT_REQUIRED))
        {
            Show("ErrSAU");
            s_mode = STATUS_MODE_BROWSE;
        }
    }
    else if ((s_mode == STATUS_MODE_COMPLETE) &&
             ((int32_t)(now - s_complete_until_ms) >= 0)) ExitDiscard();
}

bool StatusController_HandleKeyEvent(const KeyEvent *event)
{
    if (!s_active || (event == NULL)) return false;
    if ((event->type != KEY_EVENT_SHORT) &&
        (event->type != KEY_EVENT_REPEAT) &&
        (event->type != KEY_EVENT_LONG)) return true;
    s_last_activity_ms = event->timestamp_ms;
    if ((event->key == KEY_ID_TARE) && (event->type == KEY_EVENT_SHORT) &&
        (s_mode != STATUS_MODE_APPLYING) && (s_mode != STATUS_MODE_SAVING))
    {
        ExitDiscard();
        return true;
    }
    if ((event->key == KEY_ID_STAR) && (event->type == KEY_EVENT_LONG) &&
        ((s_mode == STATUS_MODE_BROWSE) || (s_mode == STATUS_MODE_EDIT)))
    {
        BeginSave(event->timestamp_ms);
        return true;
    }
    if (s_mode == STATUS_MODE_EDIT)
    {
        if (((event->key == KEY_ID_STAR) || (event->key == KEY_ID_HASH)) &&
            ((event->type == KEY_EVENT_SHORT) ||
             (event->type == KEY_EVENT_REPEAT)))
        {
            Adjust(event->key);
            RenderValue();
        }
        else if ((event->key == KEY_ID_FUNCTION) &&
                 (event->type == KEY_EVENT_SHORT))
        {
            s_candidate = s_edit;
            s_confirmed = memcmp(&s_candidate, &s_original,
                                 sizeof(s_candidate)) != 0;
            s_mode = STATUS_MODE_BROWSE;
            RenderValue();
        }
        return true;
    }
    if (s_mode != STATUS_MODE_BROWSE) return true;
    if (((event->key == KEY_ID_STAR) || (event->key == KEY_ID_HASH)) &&
        ((event->type == KEY_EVENT_SHORT) ||
         (event->type == KEY_EVENT_REPEAT)))
    {
        s_item = (event->key == KEY_ID_HASH) ?
            (StatusItem)(((uint32_t)s_item + 1U) % STATUS_ITEM_COUNT) :
            (StatusItem)(((uint32_t)s_item + STATUS_ITEM_COUNT - 1U) %
                         STATUS_ITEM_COUNT);
        ShowItem(event->timestamp_ms);
    }
    else if ((event->key == KEY_ID_FUNCTION) &&
             (event->type == KEY_EVENT_SHORT))
    {
        if (IsEditable(s_item))
        {
            s_edit = s_candidate;
            s_mode = STATUS_MODE_EDIT;
            RenderValue();
        }
        else ShowItem(event->timestamp_ms);
    }
    return true;
}

bool StatusController_IsActive(void) { return s_active; }
StatusItem StatusController_GetItem(void) { return s_item; }
bool StatusController_IsEditing(void) { return s_mode == STATUS_MODE_EDIT; }
#if defined(STAGE2A_HOST_TEST)
bool StatusController_GetVisibleCommunication(CommunicationConfig *config)
{
    if (!s_active || (config == NULL)) return false;
    *config = s_mode == STATUS_MODE_EDIT ? s_edit : s_candidate;
    return true;
}
#endif
