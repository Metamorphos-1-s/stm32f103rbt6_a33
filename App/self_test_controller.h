#ifndef SELF_TEST_CONTROLLER_H
#define SELF_TEST_CONTROLLER_H

#include <stdbool.h>

typedef enum
{
    SELF_TEST_IDLE = 0,
    SELF_TEST_DISPLAY_CLEAR,
    SELF_TEST_DIGIT_WALK,
    SELF_TEST_UP_LED_WALK,
    SELF_TEST_DOWN_LED_WALK,
    SELF_TEST_INTERNAL_BEEP,
    SELF_TEST_SHOW_VERSION,
    SELF_TEST_COMPLETE,
    SELF_TEST_FAILED
} SelfTestState;

void SelfTestController_Init(void);
bool SelfTestController_Begin(void);
void SelfTestController_Process10ms(void);
void SelfTestController_Cancel(void);
SelfTestState SelfTestController_GetState(void);

#endif /* SELF_TEST_CONTROLLER_H */
