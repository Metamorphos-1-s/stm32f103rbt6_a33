#include "event_queue.h"

#include "project_config.h"

#include <stddef.h>

static AppEvent s_queue[EVENT_QUEUE_CAPACITY];
static uint8_t s_head;
static uint8_t s_tail;
static uint8_t s_count;
static uint32_t s_dropped_count;

void EventQueue_Init(void)
{
  s_head = 0U;
  s_tail = 0U;
  s_count = 0U;
  s_dropped_count = 0U;
}

bool EventQueue_Push(const AppEvent *event)
{
  if ((event == NULL) || (s_count >= EVENT_QUEUE_CAPACITY))
  {
    ++s_dropped_count;
    return false;
  }

  s_queue[s_tail] = *event;
  s_tail = (uint8_t)((s_tail + 1U) % EVENT_QUEUE_CAPACITY);
  ++s_count;
  return true;
}

bool EventQueue_Pop(AppEvent *event)
{
  if ((event == NULL) || (s_count == 0U))
  {
    return false;
  }

  *event = s_queue[s_head];
  s_head = (uint8_t)((s_head + 1U) % EVENT_QUEUE_CAPACITY);
  --s_count;
  return true;
}

uint16_t EventQueue_Count(void)
{
  return (uint16_t)s_count;
}

uint32_t EventQueue_DroppedCount(void)
{
  return s_dropped_count;
}
