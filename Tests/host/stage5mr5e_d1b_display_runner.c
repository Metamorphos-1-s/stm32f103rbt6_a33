#include "active_display_follower.h"
#include <stdio.h>
#include <string.h>
int main(int argc,char **argv){ActiveDisplayFollower f;ActiveDisplayFollowerInput i;ActiveDisplayFollowerOutput o;long desired,baseline;unsigned long now;int stable,active,valid;
 if(argc==2&&strcmp(argv[1],"--sizeof")==0){printf("%u\n",(unsigned)sizeof(f));return 0;}ActiveDisplayFollower_Reset(&f);puts("desired_display_count,actual_display_count,anchor_count,confirmation_count,release_reason,locked,large_step");
 while(scanf("%ld,%ld,%lu,%hhu,%d,%d,%d",&desired,&baseline,&now,&i.source,&stable,&active,&valid)==7){i.desired_count=(int32_t)desired;i.baseline_count=(int32_t)baseline;i.now_ms=(uint32_t)now;i.stable=stable!=0;i.active=active!=0;i.valid=valid!=0;if(!ActiveDisplayFollower_Process(&f,&i,&o))return 2;printf("%ld,%ld,%ld,%u,%u,%u,%u\n",(long)o.desired_count,(long)o.display_count,(long)o.anchor_count,o.confirmation_count,o.release_reason,o.locked?1U:0U,o.large_step?1U:0U);}return 0;}
