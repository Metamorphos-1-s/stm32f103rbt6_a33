#include "menu_controller.h"

#include "bsp_time.h"
#include "command_service.h"
#include "config_edit.h"
#include "display_controller.h"
#include "display_codes.h"
#include "project_config.h"
#include "system_context.h"

#include <stddef.h>

static const char s_labels[MENU_ITEM_COUNT][6] = {
    {'C','A','L',' ',' ',' '}, {'C','A','P',' ',' ',' '},
    {'d','I','U',' ',' ',' '}, {'d','P',' ',' ',' ',' '},
    {'F','I','L','t',' ',' '}, {'S','t','A','b',' ',' '},
    {'Z','r','n','G',' ',' '}, {'O','L',' ',' ',' ',' '},
    {'b','r','I','G','H','t'}, {'S','P','d',' ',' ',' '},
    {'G','A','I','n',' ',' '}, {'t','r','r','E','t',' '},
    {'S','A','U','E',' ',' '}, {'r','E','S','E','t',' '},
    {'E','H','I','t',' ',' '}
};

static MenuItem s_item;
static ConfigFieldId s_field;
static int32_t s_value;
static int32_t s_step;
static uint32_t s_last_activity_ms;
static bool s_active;
static bool s_editing;
static bool s_calibration_request;
static bool s_exit_request;
static bool s_factory_confirmation;

static bool MenuController_ItemField(MenuItem item, ConfigFieldId *field,
                                     int32_t *value)
{
    const SystemContext *context = SystemContext_Get();
    if ((context == NULL) || (field == NULL) || (value == NULL)) return false;
    switch (item)
    {
        case MENU_ITEM_CAPACITY: *field=CONFIG_FIELD_CAPACITY; *value=(int32_t)context->config.metrology.capacity; break;
        case MENU_ITEM_DIVISION: *field=CONFIG_FIELD_DIVISION; *value=(int32_t)context->config.metrology.division; break;
        case MENU_ITEM_DECIMALS: *field=CONFIG_FIELD_DECIMAL_PLACES; *value=context->config.metrology.decimal_places; break;
        case MENU_ITEM_FILTER: *field=CONFIG_FIELD_FILTER_MODE; *value=context->config.metrology.filter_mode; break;
        case MENU_ITEM_STABILITY: *field=CONFIG_FIELD_STABILITY_HOLD_MS; *value=(int32_t)context->config.stability.stable_hold_ms; break;
        case MENU_ITEM_ZERO_RANGE: *field=CONFIG_FIELD_ZERO_RANGE; *value=(int32_t)context->config.metrology.zero_range; break;
        case MENU_ITEM_OVERLOAD: *field=CONFIG_FIELD_OVERLOAD_THRESHOLD; *value=(int32_t)context->config.metrology.overload_threshold; break;
        case MENU_ITEM_BRIGHTNESS: *field=CONFIG_FIELD_DISPLAY_BRIGHTNESS; *value=context->config.display.brightness; break;
        case MENU_ITEM_TARE_RETENTION: *field=CONFIG_FIELD_TARE_RETENTION; *value=context->config.system.tare_power_loss_retention ? 1 : 0; break;
        default: return false;
    }
    return true;
}

static void MenuController_FormatValue(int32_t value, char text[6])
{
    uint32_t magnitude = (value < 0) ? (0U - (uint32_t)value) : (uint32_t)value;
    uint8_t index;
    for (index=0U; index<6U; ++index) text[index]=' ';
    index=5U;
    do
    {
        text[index]=(char)('0'+(magnitude%10U));
        magnitude/=10U;
        if (index==0U) break;
        --index;
    } while (magnitude!=0U);
    if ((value<0) && (index>0U)) text[index]='-';
}

static void MenuController_Render(void)
{
    char text[6];
    if (s_editing)
    {
        MenuController_FormatValue(s_value, text);
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_EDIT, text);
    }
    else
    {
        (void)DisplayController_SetTextPage(DISPLAY_PAGE_MENU,
                                            s_labels[s_item]);
    }
}

static CommandResult MenuController_Command(CommandId id, int32_t value0,
                                             int32_t value1)
{
    CommandRequest request = {id, COMMAND_SOURCE_LOCAL_KEY, value0, value1, 0U};
    CommandResponse response;
    return CommandService_Execute(&request, &response);
}

void MenuController_Init(void)
{
    s_active=false; s_editing=false; s_calibration_request=false;
    s_exit_request=false; s_factory_confirmation=false;
    s_item=MENU_ITEM_CALIBRATION; s_step=1;
}

bool MenuController_Enter(void)
{
    if (s_active) return false;
    s_active=true; s_editing=false; s_factory_confirmation=false;
    s_item=MENU_ITEM_CALIBRATION;
    s_last_activity_ms=BSP_TimeNowMs(); MenuController_Render(); return true;
}

void MenuController_Process10ms(void)
{
    if (s_active && ((uint32_t)(BSP_TimeNowMs()-s_last_activity_ms)>=MENU_TIMEOUT_MS))
    {
        if (s_editing) (void)MenuController_Command(COMMAND_CANCEL_CONFIG_EDIT,0,0);
        if (s_factory_confirmation)
            (void)MenuController_Command(COMMAND_FACTORY_RESET_CANCEL,0,0);
        s_active=false; s_editing=false; s_factory_confirmation=false;
        s_exit_request=true;
    }
}

bool MenuController_HandleKeyEvent(const KeyEvent *event)
{
    DisplayCode code;
    char text[6];
    if (!s_active || (event==NULL) ||
        ((event->type!=KEY_EVENT_SHORT) &&
         (event->type!=KEY_EVENT_REPEAT) &&
         (event->type!=KEY_EVENT_LONG))) return false;
    s_last_activity_ms=event->timestamp_ms;
    if (s_factory_confirmation)
    {
        if ((event->key==KEY_ID_FUNCTION) &&
            (event->type==KEY_EVENT_LONG))
        {
            (void)MenuController_Command(COMMAND_FACTORY_RESET_CONFIRM,0,0);
            s_factory_confirmation=false;
        }
        else if ((event->key==KEY_ID_TARE) &&
                 (event->type==KEY_EVENT_SHORT))
        {
            (void)MenuController_Command(COMMAND_FACTORY_RESET_CANCEL,0,0);
            s_factory_confirmation=false;
            MenuController_Render();
        }
        return true;
    }
    if ((event->key==KEY_ID_FUNCTION) && (event->type==KEY_EVENT_LONG))
    {
        if (s_editing)
            (void)MenuController_Command(COMMAND_CANCEL_CONFIG_EDIT,0,0);
        s_editing=false; s_active=false; s_exit_request=true;
        return true;
    }
    if (s_editing)
    {
        if ((event->key==KEY_ID_STAR)||(event->key==KEY_ID_HASH))
        {
            int32_t next;
            if (s_field==CONFIG_FIELD_FILTER_MODE)
            {
                int32_t delta=(event->key==KEY_ID_HASH)?1:-1;
                next=(s_value+delta+(int32_t)FILTER_MODE_COUNT)%
                     (int32_t)FILTER_MODE_COUNT;
                if (MenuController_Command(COMMAND_SET_CONFIG_FIELD,s_field,next)==COMMAND_RESULT_OK)
                {
                    int32_t strength=(next==FILTER_MODE_NONE)?0:2;
                    if (MenuController_Command(COMMAND_SET_CONFIG_FIELD,
                        CONFIG_FIELD_FILTER_STRENGTH,strength)==COMMAND_RESULT_OK)
                        s_value=next;
                }
            }
            else
            {
                next=s_value+((event->key==KEY_ID_HASH)?s_step:-s_step);
                if (next>=0 && MenuController_Command(COMMAND_SET_CONFIG_FIELD,s_field,next)==COMMAND_RESULT_OK)
                    s_value=next;
            }
        }
        else if (event->key==KEY_ID_ZERO)
        {
            s_step=(s_step>=1000)?1:(s_step*10);
        }
        else if (event->key==KEY_ID_FUNCTION)
        {
            if (MenuController_Command(COMMAND_COMMIT_CONFIG_EDIT,0,0)==COMMAND_RESULT_OK)
            {
                s_editing=false;
                MenuController_Render();
                code=DISPLAY_CODE_DONE;
                if(DisplayCodes_Get(code,text))
                    DisplayController_ShowMessage(text,UI_MESSAGE_DEFAULT_MS);
                return true;
            }
        }
        else if (event->key==KEY_ID_TARE)
        { (void)MenuController_Command(COMMAND_CANCEL_CONFIG_EDIT,0,0); s_editing=false; }
        MenuController_Render(); return true;
    }
    if ((event->key==KEY_ID_STAR)||(event->key==KEY_ID_HASH))
    {
        if (event->key==KEY_ID_HASH) s_item=(MenuItem)(((uint32_t)s_item+1U)%MENU_ITEM_COUNT);
        else s_item=(MenuItem)(((uint32_t)s_item+MENU_ITEM_COUNT-1U)%MENU_ITEM_COUNT);
    }
    else if (event->key==KEY_ID_TARE)
    { s_active=false; s_exit_request=true; }
    else if (event->key==KEY_ID_FUNCTION)
    {
        if (s_item==MENU_ITEM_CALIBRATION) { s_active=false; s_calibration_request=true; }
        else if (s_item==MENU_ITEM_EXIT) { s_active=false; s_exit_request=true; }
        else if (s_item==MENU_ITEM_SAVE)
        {
            CommandResult result=MenuController_Command(
                COMMAND_REQUEST_CONFIG_SAVE,0,0);
            if((result!=COMMAND_RESULT_ACCEPTED) &&
               (result!=COMMAND_RESULT_OK) &&
               DisplayCodes_Get(DISPLAY_CODE_SAVE_ERROR,text))
                DisplayController_ShowMessage(text,UI_MESSAGE_DEFAULT_MS);
            return true;
        }
        else if (s_item==MENU_ITEM_FACTORY_RESET)
        {
            if (MenuController_Command(COMMAND_FACTORY_RESET_REQUEST,0,0)==
                COMMAND_RESULT_ACCEPTED)
            {
                s_factory_confirmation=true;
                if(DisplayCodes_Get(DISPLAY_CODE_RESET_QUERY,text))
                    DisplayController_ShowMessage(text,UI_MESSAGE_DEFAULT_MS);
            }
            return true;
        }
        else if ((s_item==MENU_ITEM_SAMPLE_RATE)||(s_item==MENU_ITEM_GAIN))
        {
            if(DisplayCodes_Get(DISPLAY_CODE_NO_SAVE,text))
                DisplayController_ShowMessage(text,UI_MESSAGE_DEFAULT_MS);
            return true;
        }
        else if (MenuController_ItemField(s_item,&s_field,&s_value) &&
                 (MenuController_Command(COMMAND_BEGIN_CONFIG_EDIT,0,0)==COMMAND_RESULT_OK))
        { s_editing=true; s_step=1; }
    }
    MenuController_Render(); return true;
}

void MenuController_Cancel(void)
{ if(s_editing) (void)MenuController_Command(COMMAND_CANCEL_CONFIG_EDIT,0,0); if(s_factory_confirmation) (void)MenuController_Command(COMMAND_FACTORY_RESET_CANCEL,0,0); s_active=false; s_editing=false; s_factory_confirmation=false; }
bool MenuController_IsActive(void) { return s_active; }
bool MenuController_TakeCalibrationRequest(void) { bool value=s_calibration_request; s_calibration_request=false; return value; }
bool MenuController_TakeExitRequest(void) { bool value=s_exit_request; s_exit_request=false; return value; }
MenuItem MenuController_GetItem(void) { return s_item; }
