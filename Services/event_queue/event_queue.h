#ifndef EVENT_QUEUE_H
#define EVENT_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
  EVENT_NONE = 0,
  EVENT_SYSTEM_STARTED,
  EVENT_CONFIG_LOADED,
  EVENT_CONFIG_CHANGED,
  EVENT_KEY,
  EVENT_NEW_ADC_SAMPLE,
  EVENT_NEW_WEIGHT_SAMPLE,
  EVENT_WEIGHT_STABLE_CHANGED,
  EVENT_LIMIT_STATE_CHANGED,
  EVENT_UART_FRAME_RECEIVED,
  EVENT_BLE_STATE_CHANGED,
  EVENT_FAULT_RAISED,
  EVENT_FAULT_CLEARED,
  EVENT_CS1237_SAMPLE_AVAILABLE,
  EVENT_TM1628_KEY_RAW_CHANGED,
  EVENT_BATTERY_SAMPLE_UPDATED,
  EVENT_W02_PWRKEY_PULSE_DONE,
  EVENT_DRIVER_READY,
  EVENT_DRIVER_ERROR,
  EVENT_RAW_MEASUREMENT_UPDATED,
  EVENT_CONFIG_SAVE_STARTED,
  EVENT_CONFIG_SAVE_COMPLETED,
  EVENT_CONFIG_SAVE_NO_CHANGE,
  EVENT_CONFIG_SAVE_FAILED,
  EVENT_CONFIG_LOAD_RECOVERED,
  EVENT_CONFIG_LOAD_DEFAULTS,
  EVENT_FACTORY_RESET_STARTED,
  EVENT_FACTORY_RESET_COMPLETED,
  EVENT_FACTORY_RESET_FAILED
} EventType;

typedef struct
{
  EventType type;
  uint32_t timestamp_ms;
  uint32_t arg0;
  uint32_t arg1;
  void *source;
} AppEvent;

/*
 * Stage 2A event arguments:
 * CS1237_AVAILABLE: arg0=buffered count; TM1628_KEY: arg0=5-bit raw mask;
 * BATTERY_UPDATED: arg0=mV, arg1=raw average; W02_DONE: arg0=low time ms.
 * DRIVER events use arg0 as a driver/error bit mask. source remains NULL.
 * RAW_MEASUREMENT_UPDATED: arg0 is the int32 raw bit pattern and must be cast
 * back to int32_t; arg1 is the accepted sample count. It is rate-limited to 20 ms.
 */

/* Main-context only. ISR producers need a critical section or a separate API. */
void EventQueue_Init(void);
bool EventQueue_Push(const AppEvent *event);
bool EventQueue_Pop(AppEvent *event);
uint16_t EventQueue_Count(void);
uint32_t EventQueue_DroppedCount(void);

#endif /* EVENT_QUEUE_H */
