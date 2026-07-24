#ifndef CALIBRATION_CONTROLLER_H
#define CALIBRATION_CONTROLLER_H

#include "calibration_model.h"
#include "key_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    CAL_STATE_IDLE = 0,
    CAL_STATE_CONFIRM_EMPTY,
    CAL_STATE_WAIT_ZERO_STABLE,
    CAL_STATE_CAPTURE_ZERO,
    CAL_STATE_INPUT_SPAN_WEIGHT,
    CAL_STATE_PROMPT_LOAD_WEIGHT,
    CAL_STATE_WAIT_SPAN_STABLE,
    CAL_STATE_CAPTURE_SPAN,
    CAL_STATE_PREVIEW,
    CAL_STATE_COMMIT_RAM,
    CAL_STATE_COMPLETE,
    CAL_STATE_CANCELLED,
    CAL_STATE_ERROR
} CalibrationState;

typedef struct
{
    CalibrationState state;
    int32_t captured_raw_zero;
    int32_t captured_raw_span;
    WeightValue span_weight;
    uint32_t zero_sample_sequence;
    uint32_t span_sample_sequence;
    CalibrationResult result;
    CalibrationConfig candidate;
    uint32_t state_enter_ms;
    bool active;
} CalibrationSession;

bool CalibrationController_Begin(void);
void CalibrationController_Process10ms(void);
bool CalibrationController_HandleKeyEvent(const KeyEvent *event);
void CalibrationController_Cancel(void);
CalibrationState CalibrationController_GetState(void);
const CalibrationSession *CalibrationController_GetSession(void);

#endif /* CALIBRATION_CONTROLLER_H */
