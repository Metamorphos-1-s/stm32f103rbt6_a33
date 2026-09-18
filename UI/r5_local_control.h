#ifndef R5_LOCAL_CONTROL_H
#define R5_LOCAL_CONTROL_H

#include "project_config.h"

#if (A33_ENABLE_STAGE5MR5_BETA != 0U)

#include "reference_lock_drift_compensator.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    R5_LOCAL_CHOICE_OFF = 0,
    R5_LOCAL_CHOICE_SHADOW,
    R5_LOCAL_CHOICE_STATIC,
    R5_LOCAL_CHOICE_DOSING,
    R5_LOCAL_CHOICE_COUNT
} R5LocalChoice;

typedef enum
{
    R5_LOCAL_RESULT_OK = 0,
    R5_LOCAL_RESULT_BUSY,
    R5_LOCAL_RESULT_ERROR
} R5LocalResult;

typedef enum
{
    R5_LOCAL_APPLICATION_SHADOW = 0,
    R5_LOCAL_APPLICATION_ACTIVE = 1
} R5LocalApplication;

typedef struct
{
    R5LocalApplication application;
    R5DriftMode mode;
    R5DriftState state;
    bool limited;
} R5LocalStatus;

bool R5LocalControl_GetStatus(R5LocalStatus *status);
bool R5LocalControl_BeginSession(void);
void R5LocalControl_EndSession(void);
bool R5LocalControl_Begin(bool config_candidate_changed);
void R5LocalControl_Adjust(bool increment);
void R5LocalControl_Confirm(void);
void R5LocalControl_Cancel(void);
bool R5LocalControl_HasCandidate(void);
R5LocalChoice R5LocalControl_GetChoice(void);
R5LocalResult R5LocalControl_Apply(void);
const char *R5LocalControl_ChoiceText(R5LocalChoice choice);
const char *R5LocalControl_StateText(const R5LocalStatus *status);
uint32_t R5LocalControl_GetFailureCount(void);

#endif

#endif
