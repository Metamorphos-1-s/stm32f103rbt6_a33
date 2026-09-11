"""Decode the 32-bit Modbus FaultManager mask."""

FAULT_NAMES = {
    1: "CONFIG_INVALID", 2: "EVENT_QUEUE_OVERFLOW", 3: "SCHEDULER_ERROR",
    4: "ADC_ERROR", 5: "CS1237_NOT_READY", 6: "CS1237_DATA_ERROR",
    7: "CALIBRATION_INVALID", 8: "UART_ERROR", 9: "BATTERY_OVERVOLTAGE",
    10: "BATTERY_LOW", 11: "MICROSECOND_TIMER_INIT", 12: "CS1237_CONFIG_ERROR",
    13: "CS1237_BUFFER_OVERRUN", 14: "TM1628_COMM_ERROR", 15: "BATTERY_ADC_INVALID",
    16: "W02_PWRKEY_SAFETY", 17: "METROLOGY_CONFIG_INVALID", 18: "CALIBRATION_DATA_CORRUPT",
    19: "WEIGHT_MATH_OVERFLOW", 20: "UI_KEY_MAP_INVALID", 21: "UI_DISPLAY_ERROR",
    22: "CONFIG_APPLY_INCONSISTENT", 23: "UI_STATE_ERROR", 24: "CONFIG_FLASH_LAYOUT",
    25: "CONFIG_FLASH_IO", 26: "CONFIG_RECORD_CORRUPT", 27: "CONFIG_SCHEMA_UNSUPPORTED",
    28: "CONFIG_SAVE_FAILED", 29: "CONFIG_SAVE_POWER_INTERRUPTED",
}

def decode(words, word_order="high"):
    if len(words) != 2:
        raise ValueError("fault mask requires exactly two registers")
    mask = ((words[0] << 16) | words[1]) if word_order == "high" else ((words[1] << 16) | words[0])
    return mask, [name for code, name in FAULT_NAMES.items() if mask & (1 << (code - 1))]
