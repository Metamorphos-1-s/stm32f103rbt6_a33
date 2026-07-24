#ifndef KEY_SERVICE_H
#define KEY_SERVICE_H

#include "key_map.h"
#include "key_types.h"
#include "project_config.h"

#include <stdbool.h>
#include <stdint.h>

bool KeyService_Init(const KeyMap *map);
void KeyService_Process10ms(uint8_t raw_key_mask, uint32_t timestamp_ms);
bool KeyService_TryPopEvent(KeyEvent *event);
uint32_t KeyService_GetDroppedEventCount(void);
bool KeyService_IsConflictActive(void);
uint32_t KeyService_GetMultiKeyConflictCount(void);
uint8_t KeyService_GetLastConflictMask(void);
uint8_t KeyService_GetLastRawMask(void);
uint8_t KeyService_GetLastLogicalMask(void);

#endif /* KEY_SERVICE_H */
