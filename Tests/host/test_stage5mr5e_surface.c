#include "menu_types.h"
#include "status_controller.h"
#include "project_config.h"

#include <stdio.h>

#define CHECK(condition) do { if (!(condition)) { \
    (void)fprintf(stderr, "CHECK failed line %d: %s\n", __LINE__, #condition); \
    return 1; } } while (0)

int main(void)
{
#if (A33_ENABLE_STAGE5MR5_BETA != 0U)
    CHECK(FW_RELEASE_VERSION == 0x0513U);
    CHECK(MENU_ITEM_R5_DRIFT > MENU_ITEM_TARE_RETENTION);
    CHECK(STATUS_ITEM_R5_STATE > STATUS_ITEM_BATTERY);
    CHECK(STATUS_ITEM_COUNT == 14U);
#else
    CHECK(FW_RELEASE_VERSION == 0x0510U);
    CHECK(STATUS_ITEM_COUNT == 13U);
#endif
    return 0;
}
