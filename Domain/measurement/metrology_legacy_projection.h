#ifndef METROLOGY_LEGACY_PROJECTION_H
#define METROLOGY_LEGACY_PROJECTION_H

#include "device_config.h"

#include <stdbool.h>

bool MetrologyLegacyProjection_Update(MetrologyConfig *config);
bool MetrologyLegacyStabilityProjection_Update(
    const MetrologyConfig *metrology, StabilityConfig *stability);
bool CalibrationLegacyProjection_Update(CalibrationConfig *config,
    MassUnit unit, const UnitDisplayConfig *display);

#endif /* METROLOGY_LEGACY_PROJECTION_H */
