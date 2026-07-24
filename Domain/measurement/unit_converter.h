#ifndef UNIT_CONVERTER_H
#define UNIT_CONVERTER_H

#include "unit_types.h"

bool UnitConverter_CountToMass(int64_t count, MassUnit unit,
                               uint8_t decimal_places,
                               MassValueUg *mass_ug);
bool UnitConverter_MassToDisplay(MassValueUg mass_ug, MassUnit unit,
                                 const UnitDisplayConfig *config,
                                 DisplayWeightValue *display);
bool UnitConverter_ValidateDisplayConfig(MassValueUg capacity_ug,
                                         MassUnit unit,
                                         const UnitDisplayConfig *config);

#endif /* UNIT_CONVERTER_H */
