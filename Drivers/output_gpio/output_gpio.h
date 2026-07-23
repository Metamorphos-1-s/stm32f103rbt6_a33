#ifndef OUTPUT_GPIO_H
#define OUTPUT_GPIO_H

#include <stdbool.h>

typedef enum
{
    OUTPUT_INTERNAL_BUZZER = 0,
    OUTPUT_EXTERNAL_BUZZER,
    OUTPUT_GREEN_LAMP,
    OUTPUT_RED_LAMP,
    OUTPUT_YELLOW_LAMP,
    OUTPUT_COUNT
} OutputId;

void OutputGpio_Init(void);
bool OutputGpio_Set(OutputId output, bool enabled);
bool OutputGpio_Get(OutputId output);
void OutputGpio_AllOff(void);

#endif /* OUTPUT_GPIO_H */
