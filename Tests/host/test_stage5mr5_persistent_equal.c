#include "default_config.h"
#include "persistent_codec.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

#define DETECT(expression) do { b = a; expression; \
    CHECK(!PersistentCodec_DeviceConfigEqual(&a, &b)); } while (0)

int main(void)
{
    DeviceConfig a,b;
    RuntimeState ar={0},br={0};
    DefaultConfig_Load(&a); b=a;
    CHECK(PersistentCodec_DeviceConfigEqual(&a,&b));
    DETECT(b.metrology.capacity_ug++);
    DETECT(b.metrology.unit_display[0].division_digit++);
    DETECT(b.metrology.load_cell.sensitivity_uv_per_v++);
    DETECT(b.metrology.profiles[1].stability_hold_ms++);
    DETECT(b.calibration.raw_zero++);
    DETECT(b.stability.enter_threshold++);
    DETECT(b.communication.baud_rate=9600U);
    DETECT(b.bluetooth.protocol_version++);
    DETECT(b.alarm.lower_limit_ug++);
    DETECT(b.display.brightness++);
    DETECT(b.battery.low_warning_mv++);
    DETECT(b.system.startup_auto_zero_enable=!b.system.startup_auto_zero_enable);
    b=a; ar.weight_view=WEIGHT_VIEW_NET; br=ar;
    CHECK(PersistentCodec_ConfigEqual(&a,&ar,&b,&br));
    br.weight_view=WEIGHT_VIEW_GROSS;
    CHECK(!PersistentCodec_ConfigEqual(&a,&ar,&b,&br));
    br=ar;br.current_tare_ug=1;
    CHECK(!PersistentCodec_ConfigEqual(&a,&ar,&b,&br));
    br=ar;br.tare_active=true;
    CHECK(!PersistentCodec_ConfigEqual(&a,&ar,&b,&br));
    b=a;b.display.reserved[0]=1;b.alarm.lower_limit=123;
    br=ar;br.config_dirty=true;br.boot_count=99;br.current_tare=123;
    CHECK(PersistentCodec_ConfigEqual(&a,&ar,&b,&br));
    return 0;
}
