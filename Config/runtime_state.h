#ifndef RUNTIME_STATE_H
#define RUNTIME_STATE_H

#include "mass_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    WEIGHT_VIEW_NET = 0,
    WEIGHT_VIEW_GROSS,
    WEIGHT_VIEW_COUNT
} WeightViewMode;

typedef struct
{
    WeightViewMode weight_view;
    int32_t current_tare;
    MassValueUg current_tare_ug;
    bool tare_active;
    bool config_dirty;
    uint32_t boot_count;
    bool migration_pending_save;
} RuntimeState;

#endif
