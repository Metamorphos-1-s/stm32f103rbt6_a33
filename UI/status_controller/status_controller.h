#ifndef STATUS_CONTROLLER_H
#define STATUS_CONTROLLER_H

#include "key_types.h"
#include "device_config.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    STATUS_ITEM_FIRMWARE = 0,
    STATUS_ITEM_MAP,
    STATUS_ITEM_SCHEMA,
    STATUS_ITEM_PROFILE,
    STATUS_ITEM_SAMPLE_RATE,
    STATUS_ITEM_GAIN,
    STATUS_ITEM_BATTERY,
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    STATUS_ITEM_R5_STATE,
#endif
    STATUS_ITEM_PROTOCOL,
    STATUS_ITEM_ADDRESS,
    STATUS_ITEM_BAUD,
    STATUS_ITEM_PARITY,
    STATUS_ITEM_STOP_BITS,
    STATUS_ITEM_WORD_ORDER,
    STATUS_ITEM_COUNT
} StatusItem;

typedef enum
{
    STATUS_MODE_LIST = 0,
    STATUS_MODE_VIEW,
    STATUS_MODE_EDIT,
    STATUS_MODE_APPLYING,
    STATUS_MODE_SAVING,
    STATUS_MODE_MESSAGE,
    STATUS_MODE_COMPLETE
} StatusMode;

void StatusController_Init(void);
bool StatusController_Enter(void);
void StatusController_Cancel(void);
void StatusController_Process10ms(void);
bool StatusController_HandleKeyEvent(const KeyEvent *event);
bool StatusController_IsActive(void);
StatusItem StatusController_GetItem(void);
StatusMode StatusController_GetMode(void);
bool StatusController_IsEditing(void);
#if defined(STAGE2A_HOST_TEST)
bool StatusController_GetVisibleCommunication(CommunicationConfig *config);
#endif

#endif
