#ifndef RUN_KEY_POLICY_H
#define RUN_KEY_POLICY_H

#include "key_types.h"

typedef enum
{
    RUN_STAR_ACTION_NONE = 0,
    RUN_STAR_ACTION_ENTER_STATUS
} RunStarAction;

RunStarAction RunKeyPolicy_GetStarAction(const KeyEvent *event);

#endif
