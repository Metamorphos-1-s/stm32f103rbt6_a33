#include "run_key_policy.h"

#include <stddef.h>

RunStarAction RunKeyPolicy_GetStarAction(const KeyEvent *event)
{
    if ((event == NULL) || (event->key != KEY_ID_STAR))
        return RUN_STAR_ACTION_NONE;
    return event->type == KEY_EVENT_LONG ? RUN_STAR_ACTION_ENTER_STATUS :
                                           RUN_STAR_ACTION_NONE;
}
