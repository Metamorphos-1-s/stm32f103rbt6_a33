#ifndef STAGE2B_BOARD_DIAGNOSTICS_H
#define STAGE2B_BOARD_DIAGNOSTICS_H

#include "output_gpio.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    STAGE2B_DIAG_STATE_DISABLED = 0,
    STAGE2B_DIAG_STATE_STARTUP,
    STAGE2B_DIAG_STATE_SEGMENT_TEST,
    STAGE2B_DIAG_STATE_UP_LED_TEST,
    STAGE2B_DIAG_STATE_DOWN_LED_TEST,
    STAGE2B_DIAG_STATE_DIGIT_TEST,
    STAGE2B_DIAG_STATE_LIVE_RAW,
    STAGE2B_DIAG_STATE_LIVE_BATTERY,
    STAGE2B_DIAG_STATE_KEY_DISPLAY,
    STAGE2B_DIAG_STATE_ERROR
} Stage2BDiagnosticState;

typedef struct
{
    Stage2BDiagnosticState state;
    int32_t latest_raw;
    uint32_t raw_sample_count;
    uint16_t cs1237_backlog;
    uint32_t cs1237_overrun_count;
    uint8_t raw_key_mask;
    uint16_t battery_raw;
    uint32_t battery_mv;
    uint8_t cs1237_config_register;
    uint32_t tm1628_error_count;
    bool output_test_active;
    OutputId active_output;
    bool w02_pulse_busy;
} Stage2BDiagnosticSnapshot;

void Stage2B_DiagnosticsInit(void);
void Stage2B_DiagnosticsProcess(void);
bool Stage2B_DiagnosticsIsActive(void);
Stage2BDiagnosticState Stage2B_DiagnosticsGetState(void);
bool Stage2B_DiagnosticsRequestLampTest(OutputId output,
                                        uint32_t duration_ms);
bool Stage2B_DiagnosticsRequestOutputTest(OutputId output,
                                          uint32_t duration_ms);
bool Stage2B_DiagnosticsRequestW02Pulse(uint32_t duration_ms);
void Stage2B_DiagnosticsStopAllOutputs(void);
void Stage2B_DiagnosticsStop(void);
void Stage2B_DiagnosticsEnterFault(void);
bool Stage2B_DiagnosticsSelectState(Stage2BDiagnosticState state);
void Stage2B_DiagnosticsSetRawKeyMask(uint8_t key_mask);
void Stage2B_DiagnosticsUpdateCs1237Stats(uint16_t backlog,
                                         uint32_t overrun_count);
const Stage2BDiagnosticSnapshot *Stage2B_DiagnosticsGetSnapshot(void);

bool Stage2B_DisplayText6(const char text[6], uint8_t decimal_point_mask);
bool Stage2B_DisplayHex24(uint32_t raw24);
bool Stage2B_DisplayBatteryMv(uint32_t battery_mv);

#endif /* STAGE2B_BOARD_DIAGNOSTICS_H */
