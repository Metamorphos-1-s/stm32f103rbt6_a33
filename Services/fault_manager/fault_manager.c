#include "fault_manager.h"

static uint64_t s_fault_mask;

static uint64_t FaultManager_Bit(FaultCode fault)
{
  uint32_t code = (uint32_t)fault;

  if ((code == 0U) || (code > 64U))
  {
    return 0U;
  }
  return (UINT64_C(1) << (code - 1U));
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
  uint64_t bit = FaultManager_Bit(fault);
  return ((bit != 0U) && ((s_fault_mask & bit) != 0U));
}

uint32_t FaultManager_GetActiveMask(void)
{
  /* Communication-only faults above bit 31 must not stop local weighing. */
  return (uint32_t)(s_fault_mask & ((UINT64_C(1) << 29U) - 1U));
}

bool FaultManager_FaultInvalidatesWeight(FaultCode fault)
{
  switch (fault)
  {
    case FAULT_ADC_ERROR:
    case FAULT_CS1237_NOT_READY:
    case FAULT_CS1237_DATA_ERROR:
    case FAULT_CALIBRATION_INVALID:
    case FAULT_CS1237_CONFIG_ERROR:
    case FAULT_METROLOGY_CONFIG_INVALID:
    case FAULT_CALIBRATION_DATA_CORRUPT:
    case FAULT_WEIGHT_MATH_OVERFLOW:
    case FAULT_CONFIG_APPLY_INCONSISTENT:
      return true;
    default:
      return false;
  }
}

bool FaultManager_HasWeightInvalidFault(void)
{
  FaultCode fault;

  for (fault = FAULT_CONFIG_INVALID; fault <= FAULT_RS485_TX_TIMEOUT;
       fault = (FaultCode)((uint32_t)fault + 1U))
  {
    if (FaultManager_FaultInvalidatesWeight(fault) &&
        FaultManager_IsActive(fault))
    {
      return true;
    }
  }
  return false;
}
