#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>

#define CRC32_INITIAL_STATE 0xFFFFFFFFUL
#define CRC32_FINAL_XOR     0xFFFFFFFFUL

uint32_t Crc32_Calculate(const uint8_t *data, uint32_t length);
uint32_t Crc32_Update(uint32_t state, const uint8_t *data, uint32_t length);

#endif /* CRC32_H */
