#include "fault_manager.h"

static uint32_t s_fault_mask;

static uint32_t FaultManager_Bit(FaultCode fault)
{
  uint32_t code = (uint32_t)fault;

  if ((code == 0U) || (code > 32U))
  {
    return 0U;
  }
  return (1UL << (code - 1U));
}

void FaultManager_Init(void)
{
  s_fault_mask = 0U;
}

void FaultManager_Set(FaultCode fault)
{
  s_fault_mask |= FaultManager_Bit(fault);
}

void FaultManager_Clear(FaultCode fault)
{
  s_fault_mask &= ~FaultManager_Bit(fault);
}

bool FaultManager_IsActive(FaultCode fault)
{
  uint32_t bit = FaultManager_Bit(fault);
  return ((bit != 0U) && ((s_fault_mask & bit) != 0U));
}

uint32_t FaultManager_GetActiveMask(void)
{
  return s_fault_mask;
}
