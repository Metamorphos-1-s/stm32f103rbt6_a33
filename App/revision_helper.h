#ifndef REVISION_HELPER_H
#define REVISION_HELPER_H

#include <stdbool.h>
#include <stdint.h>

#define REVISION_RESERVED_VALUE 0xFFFFFFFFUL

uint32_t Revision_Next(uint32_t revision);
bool Revision_IsValid(uint32_t revision);

#endif
