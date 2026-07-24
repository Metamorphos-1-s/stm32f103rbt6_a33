#ifndef MASS_TYPES_H
#define MASS_TYPES_H

#include <stdint.h>

typedef int64_t MassValueUg;

#define MASS_UG_PER_G   INT64_C(1000000)
#define MASS_UG_PER_KG  INT64_C(1000000000)
#define MASS_UG_PER_LB  INT64_C(453592370)

typedef enum
{
    SENSOR_DIRECTION_UNKNOWN = 0,
    SENSOR_DIRECTION_POSITIVE,
    SENSOR_DIRECTION_NEGATIVE
} SensorDirection;

#endif /* MASS_TYPES_H */
