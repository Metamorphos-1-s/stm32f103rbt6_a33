#include "status_controller.h"
#include "revision_helper.h"
#include "persistent_codec.h"

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
static bool s_applied;
static bool s_apply_uncertain;
static bool s_save_uncertain;
static bool s_rollback_pending;
static bool s_wait_entry_key_release;
static bool s_suppress_display;
static StatusItem s_item;
static StatusMode s_mode;
static StatusMode s_message_return_mode;
static DisplayPage s_previous_page;
static CommunicationConfig s_original;
static CommunicationConfig s_candidate;
static CommunicationConfig s_edit;
static CommunicationConfig s_applied_candidate;
static DeviceConfig s_original_config;
static DeviceConfig s_candidate_config;
static uint32_t s_original_revision;
static uint32_t s_applied_revision;
static uint32_t s_last_activity_ms;
static uint32_t s_transaction_started_ms;
static uint32_t s_message_until_ms;

static bool IsEditable(StatusItem item) { return item >= STATUS_ITEM_ADDRESS; }
static bool IsRunPage(DisplayPage page)
{
    return (page == DISPLAY_PAGE_NET) || (page == DISPLAY_PAGE_GROSS) ||
        (page == DISPLAY_PAGE_TARE) || (page == DISPLAY_PAGE_BATTERY);
}
static void Show(const char text[6])
{
    if (!s_suppress_display)
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_STATUS, text);
}
static void ShowNumber(uint32_t value)
{
    if (s_suppress_display) return;
    if (value > 999999U) value = 999999U;
    (void)DisplayController_SetNumericPage(DISPLAY_PAGE_STATUS,
                                           (int32_t)value, 0U);
}
static void RenderLabel(void) { Show(s_labels[s_item]); }

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
                          FW_RELEASE_VERSION_MINOR), 2U); break;
        case STATUS_ITEM_MAP:
            (void)DisplayController_SetNumericPage(DISPLAY_PAGE_STATUS,
                (int32_t)(((MODBUS_REGISTER_MAP_VERSION >> 8U) & 0xFFU) *
                          100U + (MODBUS_REGISTER_MAP_VERSION & 0xFFU)), 2U);
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
                 PROTOCOL_MODE_MODBUS_RTU ? "   rtU" : "  CUSt"); break;
        case STATUS_ITEM_ADDRESS: ShowNumber((s_mode == STATUS_MODE_EDIT) ?
            s_edit.modbus_address : s_candidate.modbus_address); break;
        case STATUS_ITEM_BAUD: ShowNumber((s_mode == STATUS_MODE_EDIT) ?
            s_edit.baud_rate : s_candidate.baud_rate); break;
        case STATUS_ITEM_PARITY:
        {
            CommunicationParity parity = (s_mode == STATUS_MODE_EDIT) ?
                s_edit.parity : s_candidate.parity;
            Show(parity == COMM_PARITY_NONE ? "  nonE" :
                 parity == COMM_PARITY_EVEN ? "  EUEn" : "   Odd"); break;
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

static void ExitStatus(void)
{
    s_active = false;
    s_mode = STATUS_MODE_LIST;
    if (!s_suppress_display) DisplayController_SetPage(s_previous_page);
}

static bool OwnsTransaction(void)
{
    return (s_mode == STATUS_MODE_APPLYING) ||
        (s_mode == STATUS_MODE_SAVING) ||
        ((s_mode == STATUS_MODE_MESSAGE) &&
         ((s_message_return_mode == STATUS_MODE_APPLYING) ||
          (s_message_return_mode == STATUS_MODE_SAVING)));
}

static void ShowMessage(const char text[6], StatusMode return_mode,
                        uint32_t now_ms)
{
    if (s_suppress_display && (return_mode == STATUS_MODE_LIST))
    {
        ExitStatus();
        return;
    }
    Show(text);
    s_mode = STATUS_MODE_MESSAGE;
    s_message_return_mode = return_mode;
    s_message_until_ms = now_ms + UI_MESSAGE_DEFAULT_MS;
}

static void ShowCompletion(const char text[6], uint32_t now_ms)
{
    if (s_suppress_display)
    {
        ExitStatus();
        return;
    }
    Show(text);
    s_mode = STATUS_MODE_COMPLETE;
    s_message_until_ms = now_ms + UI_MESSAGE_DEFAULT_MS;
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

static bool AppliedCandidateIsCurrent(void)
{
    const SystemContext *context = SystemContext_Get();
    return s_applied && (context != NULL) &&
        (SystemContext_GetConfigRevision() == s_original_revision) &&
        (memcmp(&context->config.communication, &s_applied_candidate,
                sizeof(s_applied_candidate)) == 0);
}

static bool PublishedCandidateIsCurrent(void)
{
    const SystemContext *context = SystemContext_Get();
    return (context != NULL) &&
        (SystemContext_GetConfigRevision() == s_applied_revision) &&
        (SystemContext_GetSavedRevision() == s_applied_revision) &&
        PersistentCodec_DeviceConfigEqual(&context->config,
            &s_candidate_config);
}

static void RequestSaveOnly(uint32_t now_ms)
{
    CommandResult result;
    if (!AppliedCandidateIsCurrent())
    {
        ShowMessage(" bUSY ", STATUS_MODE_LIST, now_ms); return;
    }
    s_candidate_config = s_original_config;
    s_candidate_config.communication = s_candidate;
    result = PersistenceManager_RequestCandidateSave(&s_candidate_config,
        &s_original_config, false, s_original_revision);
    if (result == COMMAND_RESULT_ACCEPTED)
    {
        Show(" SAUE ");
        s_mode = STATUS_MODE_SAVING;
        s_transaction_started_ms = now_ms;
        s_save_uncertain = false;
    }
    else
    {
        s_rollback_pending = true;
        if (CommunicationManager_RequestLocalApply(&s_original) ==
            COMMAND_RESULT_ACCEPTED)
        {
            Show("rOLbAC");
            s_mode = STATUS_MODE_APPLYING;
            s_transaction_started_ms = now_ms;
        }
        else ShowMessage("ErrSAU", STATUS_MODE_LIST, now_ms);
    }
}

static void BeginSave(uint32_t now_ms)
{
    if (s_mode == STATUS_MODE_EDIT) s_mode = STATUS_MODE_LIST;
    if (!s_confirmed ||
        (memcmp(&s_candidate, &s_original, sizeof(s_candidate)) == 0))
    { ShowCompletion("noCHG ", now_ms); return; }
    if (s_save_uncertain || s_apply_uncertain)
    { ShowMessage(" UnC  ", s_save_uncertain ? STATUS_MODE_SAVING :
                  STATUS_MODE_APPLYING, now_ms); return; }
    if (s_applied) { RequestSaveOnly(now_ms); return; }
    if (!CommunicationManager_IsConfigValid(&s_candidate))
    { ShowMessage("InUALd", STATUS_MODE_LIST, now_ms); return; }
    if (SystemContext_GetConfigRevision() != s_original_revision)
    { ShowMessage(" bUSY ", STATUS_MODE_LIST, now_ms); return; }
    if (CommunicationManager_RequestLocalApply(&s_candidate) !=
        COMMAND_RESULT_ACCEPTED)
    { ShowMessage(" Err  ", STATUS_MODE_LIST, now_ms); return; }
    Show("APPLY ");
    s_mode = STATUS_MODE_APPLYING;
    s_transaction_started_ms = now_ms;
}

void StatusController_Init(void)
{ s_active = false; s_mode = STATUS_MODE_LIST; }

bool StatusController_Enter(void)
{
    const SystemContext *context = SystemContext_Get();
    if (s_active || (context == NULL)) return false;
    s_active = true;
    s_confirmed = false;
    s_applied = false;
    s_apply_uncertain = false;
    s_save_uncertain = false;
    s_rollback_pending = false;
    s_wait_entry_key_release = true;
    s_suppress_display = false;
    s_item = STATUS_ITEM_FIRMWARE;
    s_mode = STATUS_MODE_LIST;
    s_previous_page = DisplayController_GetPage();
    if (!IsRunPage(s_previous_page))
        s_previous_page = context->runtime.weight_view == WEIGHT_VIEW_GROSS ?
            DISPLAY_PAGE_GROSS : DISPLAY_PAGE_NET;
    s_original_config = context->config;
    s_candidate_config = s_original_config;
    s_original = s_original_config.communication;
    s_candidate = s_original;
    s_original_revision = SystemContext_GetConfigRevision();
    s_last_activity_ms = BSP_TimeNowMs();
    RenderLabel();
    return true;
}

void StatusController_Cancel(void)
{
    if (!s_active) return;
    if (OwnsTransaction())
    {
        /* FAULT may take over the display, but the controller must retain
           ownership until the asynchronous operation reaches a terminal state. */
        s_suppress_display = true;
        return;
    }
    ExitStatus();
}

void StatusController_Process10ms(void)
{
    uint32_t now = BSP_TimeNowMs();
    if (!s_active) return;
    if ((s_mode == STATUS_MODE_LIST) || (s_mode == STATUS_MODE_VIEW) ||
        (s_mode == STATUS_MODE_EDIT))
    {
        if ((uint32_t)(now - s_last_activity_ms) >= MENU_TIMEOUT_MS)
        { ExitStatus(); return; }
    }
    else if (s_mode == STATUS_MODE_MESSAGE)
    {
        if ((int32_t)(now - s_message_until_ms) >= 0)
        {
            s_mode = s_message_return_mode;
            if ((s_mode == STATUS_MODE_APPLYING) ||
                (s_mode == STATUS_MODE_SAVING))
                s_transaction_started_ms = now;
            if (s_mode == STATUS_MODE_LIST) RenderLabel(); else RenderValue();
        }
    }
    else if (s_mode == STATUS_MODE_APPLYING)
    {
        CommunicationApplyResult result = CommunicationManager_GetApplyResult();
        if (result == COMM_APPLY_RESULT_SUCCESS)
        {
            const SystemContext *context = SystemContext_Get();
            if (s_rollback_pending)
            {
                s_rollback_pending = false;
                s_applied = false;
                ShowMessage("ErrSAU", STATUS_MODE_LIST, now);
                return;
            }
            if ((context == NULL) ||
                (SystemContext_GetConfigRevision() != s_original_revision) ||
                (memcmp(&context->config.communication, &s_candidate,
                        sizeof(s_candidate)) != 0))
            { ShowMessage(" bUSY ", STATUS_MODE_LIST, now); return; }
            s_applied = true;
            s_applied_candidate = s_candidate;
            s_applied_revision = Revision_Next(s_original_revision);
            RequestSaveOnly(now);
        }
        else if (result == COMM_APPLY_RESULT_FAILED)
        {
            s_rollback_pending = false;
            ShowMessage(s_applied ? "ErrSAU" : " Err  ",
                        STATUS_MODE_LIST, now);
        }
        else if ((uint32_t)(now - s_transaction_started_ms) >=
                 STATUS_TRANSACTION_TIMEOUT_MS)
        {
            s_apply_uncertain = true;
            ShowMessage(" UnC  ", STATUS_MODE_APPLYING, now);
        }
    }
    else if (s_mode == STATUS_MODE_SAVING)
    {
        PersistenceStatus status = PersistenceManager_GetStatus();
        if (((status == PERSISTENCE_STATUS_SUCCESS) ||
             (status == PERSISTENCE_STATUS_NO_CHANGE)) &&
            PublishedCandidateIsCurrent())
            ShowCompletion(status == PERSISTENCE_STATUS_SUCCESS ?
                           "  donE" : "noCHG ", now);
        else if (status == PERSISTENCE_STATUS_REBOOT_REQUIRED)
        {
            s_save_uncertain = true;
            ShowMessage(" UnC  ", STATUS_MODE_SAVING, now);
        }
        else if (status == PERSISTENCE_STATUS_FAILED)
        {
            s_rollback_pending = true;
            if (CommunicationManager_RequestLocalApply(&s_original) ==
                COMMAND_RESULT_ACCEPTED)
            {
                Show("rOLbAC");
                s_mode = STATUS_MODE_APPLYING;
                s_transaction_started_ms = now;
            }
            else ShowMessage("ErrSAU", STATUS_MODE_LIST, now);
        }
        else if ((uint32_t)(now - s_transaction_started_ms) >=
                 STATUS_TRANSACTION_TIMEOUT_MS)
        {
            s_save_uncertain = true;
            ShowMessage(" UnC  ", STATUS_MODE_SAVING, now);
        }
    }
    else if ((s_mode == STATUS_MODE_COMPLETE) &&
             ((int32_t)(now - s_message_until_ms) >= 0)) ExitStatus();
}

bool StatusController_HandleKeyEvent(const KeyEvent *event)
{
    if (!s_active || (event == NULL)) return false;
    if (s_wait_entry_key_release && (event->key == KEY_ID_STAR))
    {
        if (event->type == KEY_EVENT_RELEASED)
            s_wait_entry_key_release = false;
        return true;
    }
    if ((event->type != KEY_EVENT_SHORT) &&
        (event->type != KEY_EVENT_LONG)) return true;
    s_last_activity_ms = event->timestamp_ms;
    if ((event->key == KEY_ID_FUNCTION) && (event->type == KEY_EVENT_LONG) &&
        ((s_mode == STATUS_MODE_LIST) || (s_mode == STATUS_MODE_VIEW) ||
         (s_mode == STATUS_MODE_EDIT)))
    { BeginSave(event->timestamp_ms); return true; }
    if (s_mode == STATUS_MODE_VIEW)
    {
        if ((event->key == KEY_ID_TARE) && (event->type == KEY_EVENT_SHORT))
        { s_mode = STATUS_MODE_LIST; RenderLabel(); }
        return true;
    }
    if (s_mode == STATUS_MODE_EDIT)
    {
        if (((event->key == KEY_ID_STAR) || (event->key == KEY_ID_HASH)) &&
            (event->type == KEY_EVENT_SHORT))
        { Adjust(event->key); RenderValue(); }
        else if ((event->key == KEY_ID_FUNCTION) &&
                 (event->type == KEY_EVENT_SHORT))
        {
            s_candidate = s_edit;
            s_confirmed = memcmp(&s_candidate, &s_original,
                                 sizeof(s_candidate)) != 0;
            s_mode = STATUS_MODE_LIST; RenderLabel();
        }
        else if ((event->key == KEY_ID_TARE) &&
                 (event->type == KEY_EVENT_SHORT))
        { s_mode = STATUS_MODE_LIST; RenderLabel(); }
        return true;
    }
    if (s_mode != STATUS_MODE_LIST) return true;
    if ((event->key == KEY_ID_TARE) && (event->type == KEY_EVENT_SHORT))
    { ExitStatus(); return true; }
    if (((event->key == KEY_ID_STAR) || (event->key == KEY_ID_HASH)) &&
        (event->type == KEY_EVENT_SHORT))
    {
        s_item = (event->key == KEY_ID_HASH) ?
            (StatusItem)(((uint32_t)s_item + 1U) % STATUS_ITEM_COUNT) :
            (StatusItem)(((uint32_t)s_item + STATUS_ITEM_COUNT - 1U) %
                         STATUS_ITEM_COUNT);
        RenderLabel();
    }
    else if ((event->key == KEY_ID_FUNCTION) &&
             (event->type == KEY_EVENT_SHORT))
    {
        if (IsEditable(s_item))
        { s_edit = s_candidate; s_mode = STATUS_MODE_EDIT; }
        else s_mode = STATUS_MODE_VIEW;
        RenderValue();
    }
    return true;
}

bool StatusController_IsActive(void) { return s_active; }
StatusItem StatusController_GetItem(void) { return s_item; }
StatusMode StatusController_GetMode(void) { return s_mode; }
bool StatusController_IsEditing(void) { return s_mode == STATUS_MODE_EDIT; }
#if defined(STAGE2A_HOST_TEST)
bool StatusController_GetVisibleCommunication(CommunicationConfig *config)
{
    if (!s_active || (config == NULL)) return false;
    *config = s_mode == STATUS_MODE_EDIT ? s_edit : s_candidate;
    return true;
}
#endif
