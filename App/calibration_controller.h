#ifndef CALIBRATION_CONTROLLER_H
#define CALIBRATION_CONTROLLER_H

#include "project_config.h"
#include "calibration_model.h"
#include "key_types.h"
#include "numeric_edit_cursor.h"

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
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
    , CAL_STATE_SAVE_WAIT,
    CAL_STATE_SAVE_FAILED,
    CAL_STATE_SAVE_UNCERTAIN
#endif
} CalibrationState;

typedef struct
{
    CalibrationState state;
    int32_t captured_raw_zero;
    int32_t captured_raw_span;
    MassValueUg span_mass_ug;
    int64_t span_display_count;
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION == 0U)
    MassValueUg capacity_ug_at_begin;
#endif
    MassUnit input_unit;
    uint8_t input_decimal_places;
    uint8_t input_division_digit;
    NumericEditCursor edit_cursor;
    uint16_t session_id;
    uint32_t zero_sample_sequence;
    uint32_t span_sample_sequence;
    CalibrationResult result;
    CalibrationConfig candidate;
    uint32_t state_enter_ms;
#if (A33_ENABLE_STAGE5PA2D_CALIBRATION != 0U)
    /* Expected revision before commit, then the revision being saved. */
    uint32_t transaction_revision;
#endif
    bool active;
} CalibrationSession;

bool CalibrationController_Begin(void);
void CalibrationController_Process10ms(void);
bool CalibrationController_HandleKeyEvent(const KeyEvent *event);
void CalibrationController_Cancel(void);
CalibrationState CalibrationController_GetState(void);
const CalibrationSession *CalibrationController_GetSession(void);

#endif /* CALIBRATION_CONTROLLER_H */
