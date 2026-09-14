#include "static_drift_compensator.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){printf("FAIL:%d\n",__LINE__);return 1;}}while(0)
static StaticDriftOutput Feed(StaticDriftCompensator *c,int64_t m,uint32_t t,uint32_t s){StaticDriftInput i={m,t,s,true,false,false,false};StaticDriftOutput o={0};if(!StaticDriftCompensator_Process(c,&i,&o))o.state=STATIC_DRIFT_LIMITED;return o;}
int main(void){StaticDriftConfig cfg;StaticDriftCompensator c;StaticDriftOutput o;uint32_t i;
 StaticDriftCompensator_DefaultConfig(&cfg);CHECK(cfg.maximum_update_ug*10U<=1000U);CHECK(StaticDriftCompensator_Init(&c,&cfg));
 o=Feed(&c,123456,0,1);CHECK(o.corrected_mass_ug==123456&&o.drift_offset_ug==0&&o.state==STATIC_DRIFT_DISABLED);
 StaticDriftCompensator_Enable(&c,0,0);for(i=1;i<=170;i++)o=Feed(&c,(int64_t)i*5,i*100,i);CHECK(o.state==STATIC_DRIFT_COMPENSATING);CHECK(o.drift_offset_ug>0);CHECK(o.corrected_mass_ug<850);
 StaticDriftCompensator_Reset(&c);StaticDriftCompensator_Enable(&c,500000000,0);for(i=1;i<=170;i++)o=Feed(&c,500000000-(int64_t)i*5,i*100,i);CHECK(o.drift_offset_ug<0);
 o=Feed(&c,0,17100,171);CHECK(o.state==STATIC_DRIFT_HOLD_OFF);{int64_t held=o.drift_offset_ug;for(i=172;i<250;i++)o=Feed(&c,0,i*100,i);CHECK(o.drift_offset_ug==held);}
 for(i=250;i<300;i++){int64_t m=(i<260)?0:(i<280?5000:10000);o=Feed(&c,m,i*100,i);}CHECK(o.state==STATIC_DRIFT_HOLD_OFF);CHECK(o.corrected_mass_ug>9000);
 StaticDriftCompensator_Disable(&c);o=Feed(&c,-987654,30000,300);CHECK(o.corrected_mass_ug==-987654&&o.drift_offset_ug==0);
 StaticDriftCompensator_Enable(&c,0,0);o=Feed(&c,0,100,1);o=Feed(&c,0,500,4);CHECK(o.freeze_reason==STATIC_DRIFT_FREEZE_SEQUENCE_GAP);
 {StaticDriftInput bad={0,600,5,true,false,true,false};CHECK(StaticDriftCompensator_Process(&c,&bad,&o));CHECK(o.state==STATIC_DRIFT_LIMITED&&o.freeze_reason==STATIC_DRIFT_FREEZE_FAULT);}
 StaticDriftCompensator_HandleEvent(&c,STATIC_DRIFT_EVENT_CALIBRATION_COMMIT,1000,700);CHECK(c.drift_offset_ug==0&&c.state==STATIC_DRIFT_ARMING);
 puts("stage5mr drift tests passed");return 0;}
