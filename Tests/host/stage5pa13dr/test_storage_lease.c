/* Run the existing real dual-slot/power-cut regressions in diagnostic mode. */
#define main A13DR_OriginalStorageMain
#include "../test_stage4b.c"
#undef main
#include "stage5pa13dr_stats.h"
int main(void)
{
    DeviceConfig config, loaded;
    RuntimeState runtime, loaded_runtime;
    ConfigLoadInfo info;
    A13DRStats saved;
    uint32_t mutations;
    uint32_t save_requests;
    int original=A13DR_OriginalStorageMain();
    if(original) return original;
    FakeFlash_Reset();
    MakeConfig(&config,&runtime);
    ConfigStore_Init(FakeFlash_GetBackend());
    CHECK(ConfigStore_RequestSave(&config,&runtime,1U));
    RunStore(); ConfigStore_AcknowledgeResult();
    CHECK(ConfigStore_Load(&loaded,&loaded_runtime,&info)==CONFIG_LOAD_OK);
    CHECK(loaded.display.brightness==config.display.brightness);
    CHECK(ConfigStore_ClaimA13DRStats());
    CHECK(!ConfigStore_ClaimA13DRStats());
    A13DR_Accumulate(&g_a13dr_stats,12345U,1U,0U);
    saved=g_a13dr_stats;
    CHECK(ConfigStore_Load(&loaded,&loaded_runtime,&info)==CONFIG_LOAD_IO_ERROR);
    CHECK(memcmp(&saved,&g_a13dr_stats,sizeof(saved))==0);
    ConfigStore_Init(NULL); /* Must not reinitialize a leased workspace. */
    CHECK(memcmp(&saved,&g_a13dr_stats,sizeof(saved))==0);
    config.display.brightness=4U;
    mutations=FakeFlash_GetMutationCount();
    save_requests=ConfigStore_GetStatistics()->save_request_count;
    CHECK(!ConfigStore_RequestSave(&config,&runtime,2U));
    CHECK(!ConfigStore_RequestFactoryReset(&config,&runtime,2U));
    CHECK(ConfigStore_GetActiveSequence()==1U);
    CHECK(FakeFlash_GetMutationCount()==mutations);
    CHECK(ConfigStore_GetStatistics()->save_request_count==save_requests);
    CHECK(memcmp(&saved,&g_a13dr_stats,sizeof(saved))==0);
    CHECK(ConfigStore_A13DRStatsClaimed());
    return s_failures ? 1 : 0;
}
