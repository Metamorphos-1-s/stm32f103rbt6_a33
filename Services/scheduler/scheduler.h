#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*SchedulerTaskFn)(void *context);

typedef struct
{
  SchedulerTaskFn function;
  void *context;
  uint32_t period_ms;
  uint32_t next_run_ms;
  bool enabled;
  bool registered;
} SchedulerTask;

void Scheduler_Init(void);
int32_t Scheduler_AddPeriodicTask(SchedulerTaskFn function, void *context,
                                  uint32_t period_ms,
                                  uint32_t first_delay_ms);
bool Scheduler_SetTaskEnabled(int32_t task_id, bool enabled);
void Scheduler_RunPending(void);
uint8_t Scheduler_GetTaskCount(void);

#endif /* SCHEDULER_H */
