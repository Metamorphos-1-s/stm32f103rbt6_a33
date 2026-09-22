#include "menu_types.h"
#include "status_controller.h"
#include "project_config.h"

int main(void)
{
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    _Static_assert(FW_RELEASE_VERSION == 0x0515U,
                   "Beta firmware version changed");
    _Static_assert(MENU_ITEM_R5_DRIFT > MENU_ITEM_TARE_RETENTION,
                   "R5 menu ordering changed");
    _Static_assert(STATUS_ITEM_R5_STATE > STATUS_ITEM_BATTERY,
                   "R5 status ordering changed");
    _Static_assert(STATUS_ITEM_COUNT == 14U,
                   "Beta status count changed");
#else
    _Static_assert(FW_RELEASE_VERSION == 0x0510U,
                   "Release firmware version changed");
    _Static_assert(STATUS_ITEM_COUNT == 13U,
                   "Release status count changed");
#endif
    return 0;
}
