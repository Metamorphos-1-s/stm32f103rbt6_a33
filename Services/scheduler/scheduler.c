#include "scheduler.h"

#include "bsp_time.h"
#include "project_config.h"

#include <stddef.h>

static SchedulerTask s_tasks[SCHEDULER_MAX_TASKS];
static uint8_t s_task_count;

static bool Scheduler_IsDue(uint32_t now_ms, uint32_t deadline_ms)
{
  return ((int32_t)(now_ms - deadline_ms) >= 0);
}

void Scheduler_Init(void)
{
  uint8_t index;

  s_task_count = 0U;
  for (index = 0U; index < SCHEDULER_MAX_TASKS; ++index)
  {
    s_tasks[index].function = NULL;
    s_tasks[index].context = NULL;
    s_tasks[index].period_ms = 0U;
    s_tasks[index].next_run_ms = 0U;
    s_tasks[index].enabled = false;
    s_tasks[index].registered = false;
  }
}

int32_t Scheduler_AddPeriodicTask(SchedulerTaskFn function, void *context,
                                  uint32_t period_ms,
                                  uint32_t first_delay_ms)
{
  SchedulerTask *task;
  int32_t task_id;

  if ((function == NULL) || (period_ms == 0U) ||
      (s_task_count >= SCHEDULER_MAX_TASKS))
  {
    return -1;
  }

  task_id = (int32_t)s_task_count;
  task = &s_tasks[(uint8_t)task_id];
  task->function = function;
  task->context = context;
  task->period_ms = period_ms;
  task->next_run_ms = BSP_TimeNowMs() + first_delay_ms;
  task->enabled = true;
  task->registered = true;
  ++s_task_count;
  return task_id;
}

bool Scheduler_SetTaskEnabled(int32_t task_id, bool enabled)
{
  SchedulerTask *task;

  if ((task_id < 0) || ((uint32_t)task_id >= (uint32_t)s_task_count))
  {
    return false;
  }

  task = &s_tasks[(uint8_t)task_id];
  if (enabled && !task->enabled)
  {
    task->next_run_ms = BSP_TimeNowMs() + task->period_ms;
  }
  task->enabled = enabled;
  return true;
}

void Scheduler_RunPending(void)
{
  uint8_t index;

  for (index = 0U; index < s_task_count; ++index)
  {
    SchedulerTask *task = &s_tasks[index];
    uint32_t now_ms;
    uint32_t elapsed_ms;
    uint32_t periods_to_advance;

    if (!task->registered || !task->enabled)
    {
      continue;
    }

    now_ms = BSP_TimeNowMs();
    if (!Scheduler_IsDue(now_ms, task->next_run_ms))
    {
      continue;
    }

    elapsed_ms = now_ms - task->next_run_ms;
    periods_to_advance = (elapsed_ms / task->period_ms) + 1U;
    task->next_run_ms += periods_to_advance * task->period_ms;
    task->function(task->context);
  }
}

uint8_t Scheduler_GetTaskCount(void)
{
  return s_task_count;
}
