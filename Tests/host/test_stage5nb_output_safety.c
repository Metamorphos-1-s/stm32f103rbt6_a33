#include "alarm_output_manager.h"
#include "guarded_checkweigh.h"
#include "checkweigh_shadow.h"
#include "output_gpio.h"

#include <stdio.h>
#include <string.h>

static bool s_output[OUTPUT_COUNT];
#define CHECK(x) do { if (!(x)) { (void)printf("FAIL %d: %s\n", __LINE__, #x); return 1; } } while (0)

void OutputGpio_Init(void) { OutputGpio_AllOff(); }
bool OutputGpio_Set(OutputId id, bool enabled)
{ if ((uint32_t)id >= OUTPUT_COUNT) return false; s_output[id]=enabled; return true; }
bool OutputGpio_Get(OutputId id)
{ return (uint32_t)id < OUTPUT_COUNT ? s_output[id] : false; }
void OutputGpio_AllOff(void) { (void)memset(s_output, 0, sizeof(s_output)); }

static bool AllOff(void)
{ unsigned i; for(i=0;i<OUTPUT_COUNT;++i) if(s_output[i])return false;return true; }
static bool OneLamp(void)
{ return (unsigned)s_output[OUTPUT_GREEN_LAMP]+(unsigned)s_output[OUTPUT_YELLOW_LAMP]+(unsigned)s_output[OUTPUT_RED_LAMP] <= 1U; }

static void Step(GuardedCheckweigh *guarded, AlarmOutputManager *manager,
    GuardedCheckweighInput *input, AlarmConfig *config, uint32_t sequence,
    uint32_t now)
{
    CheckweighResult result;
    input->sample_sequence=sequence;input->sample_timestamp_ms=now;
    (void)GuardedCheckweigh_Process(guarded,input,now,&result);
    (void)AlarmOutputManager_Update(manager,&result,config,now);
}

int main(void)
{
    GuardedCheckweigh guarded; AlarmOutputManager manager;
    GuardedCheckweighInput input; AlarmConfig config;
    (void)memset(&input,0,sizeof(input));(void)memset(&config,0,sizeof(config));
    input.enabled=true;input.static_class=CHECKWEIGH_SHADOW_LOW;
    input.dynamic_class=CHECKWEIGH_SHADOW_LOW;
    config.internal_buzzer_enable=true;config.external_buzzer_enable=true;
    config.qualified_beep_enable=true;
    GuardedCheckweigh_Init(&guarded);AlarmOutputManager_Init(&manager);
    Step(&guarded,&manager,&input,&config,1U,100U);CHECK(AllOff());
    CHECK(GuardedCheckweigh_SetMode(&guarded,GUARDED_CHECKWEIGH_STATIC,0U,true));
    Step(&guarded,&manager,&input,&config,1U,100U);CHECK(AllOff());
    Step(&guarded,&manager,&input,&config,2U,200U);CHECK(AllOff());
    Step(&guarded,&manager,&input,&config,3U,300U);
    CHECK(s_output[OUTPUT_YELLOW_LAMP]&&OneLamp());
    input.static_class=CHECKWEIGH_SHADOW_OK;Step(&guarded,&manager,&input,&config,4U,400U);
    CHECK(s_output[OUTPUT_GREEN_LAMP]&&s_output[OUTPUT_INTERNAL_BUZZER]&&OneLamp());
    input.static_class=CHECKWEIGH_SHADOW_HIGH;Step(&guarded,&manager,&input,&config,5U,500U);
    CHECK(s_output[OUTPUT_RED_LAMP]&&s_output[OUTPUT_INTERNAL_BUZZER]&&
          s_output[OUTPUT_EXTERNAL_BUZZER]&&OneLamp());
    input.static_class=CHECKWEIGH_SHADOW_PENDING;Step(&guarded,&manager,&input,&config,6U,600U);CHECK(AllOff());
    input.static_class=CHECKWEIGH_SHADOW_INVALID;Step(&guarded,&manager,&input,&config,7U,700U);CHECK(AllOff());
    input.static_class=CHECKWEIGH_SHADOW_HIGH;input.fault_active=true;
    Step(&guarded,&manager,&input,&config,8U,800U);CHECK(AllOff());
    input.fault_active=false;input.sample_timestamp_ms=800U;
    {CheckweighResult result;(void)GuardedCheckweigh_Process(&guarded,&input,1051U,&result);
     (void)AlarmOutputManager_Update(&manager,&result,&config,1051U);}CHECK(AllOff());
    CHECK(GuardedCheckweigh_SetMode(&guarded,GUARDED_CHECKWEIGH_OFF,
                                    guarded.generation,true));
    AlarmOutputManager_AllOff(&manager);CHECK(AllOff());
    (void)printf("Stage 5N-B output safety tests passed\n");return 0;
}
