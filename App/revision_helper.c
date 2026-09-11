#include "revision_helper.h"

uint32_t Revision_Next(uint32_t revision)
{
    uint32_t next = revision + 1U;
    return (next == REVISION_RESERVED_VALUE) ? 0U : next;
}

bool Revision_IsValid(uint32_t revision)
{
    return revision != REVISION_RESERVED_VALUE;
}
