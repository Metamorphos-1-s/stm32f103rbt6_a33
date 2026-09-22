#include "active_display_follower.h"
#include <limits.h>
#include <stddef.h>
#include <string.h>
#define FLAG_INITIALIZED 1U
#define FLAG_CATCHING_UP 2U
#define FLAG_LOCKED 4U
#define CONFIRMATION_MS 1000U
#define LARGE_STEP_DIVISIONS 8U
static uint32_t Distance(int32_t a,int32_t b){return a>=b?(uint32_t)a-(uint32_t)b:(uint32_t)b-(uint32_t)a;}
void ActiveDisplayFollower_Reset(ActiveDisplayFollower *f){if(f)(void)memset(f,0,sizeof(*f));}
static void Publish(const ActiveDisplayFollower *f,int32_t desired,bool step,ActiveDisplayFollowerOutput *o){o->desired_count=desired;o->display_count=f->display_count;o->anchor_count=f->display_count;o->confirmation_count=f->confirmation_count;o->release_reason=step?2U:0U;o->locked=(f->flags&FLAG_LOCKED)!=0U;o->large_step=step;}
bool ActiveDisplayFollower_Process(ActiveDisplayFollower *f,const ActiveDisplayFollowerInput *i,ActiveDisplayFollowerOutput *o){int32_t d;if(!f||!i||!o)return false;
 if(!(f->flags&FLAG_INITIALIZED)||!i->valid||f->source!=i->source){ActiveDisplayFollower_Reset(f);f->display_count=i->baseline_count;f->candidate_count=i->desired_count;f->candidate_start_ms=i->now_ms;f->source=i->source;f->flags=FLAG_INITIALIZED|(i->valid?FLAG_LOCKED:0U);Publish(f,i->desired_count,false,o);return true;}
 if(!i->active){f->display_count=i->baseline_count;f->candidate_count=i->desired_count;f->candidate_start_ms=i->now_ms;f->confirmation_count=0;f->flags=FLAG_INITIALIZED;Publish(f,i->desired_count,false,o);return true;}
 d=i->desired_count-f->display_count;if(Distance(i->desired_count,f->display_count)>LARGE_STEP_DIVISIONS){f->display_count=i->desired_count;f->candidate_count=i->desired_count;f->candidate_start_ms=i->now_ms;f->confirmation_count=0;f->flags=FLAG_INITIALIZED;Publish(f,i->desired_count,true,o);return true;}f->flags|=FLAG_LOCKED;
 if(d==0){f->candidate_count=i->desired_count;f->candidate_start_ms=i->now_ms;f->confirmation_count=0;f->flags&=(uint8_t)~(uint8_t)FLAG_CATCHING_UP;}
 else if(f->flags&FLAG_CATCHING_UP){f->candidate_count=i->desired_count;if(i->now_ms!=f->candidate_start_ms){f->candidate_start_ms=i->now_ms;f->display_count+=d>0?1:-1;}f->confirmation_count=0;if(f->display_count==i->desired_count)f->flags&=(uint8_t)~(uint8_t)FLAG_CATCHING_UP;}
 else if(f->candidate_count!=i->desired_count){f->candidate_count=i->desired_count;f->candidate_start_ms=i->now_ms;f->confirmation_count=1;}
 else{if(f->confirmation_count<UINT16_MAX)++f->confirmation_count;if(i->stable&&(uint32_t)(i->now_ms-f->candidate_start_ms)>=CONFIRMATION_MS){f->display_count+=d>0?1:-1;f->confirmation_count=0;if(f->display_count!=i->desired_count){f->flags|=FLAG_CATCHING_UP;f->candidate_start_ms=i->now_ms;}}}
 Publish(f,i->desired_count,false,o);return true;}
