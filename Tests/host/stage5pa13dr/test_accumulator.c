#include "stage5pa13dr_stats.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#define CHECK(x) do { if (!(x)) { (void)fprintf(stderr,"line %d: %s\n",__LINE__,#x); return 1; } } while(0)
_Static_assert(offsetof(A13DRStats,stack_offset_flags)==132U,"assembly ABI");
int main(void)
{
    A13DRStats s;
    (void)memset(&s,0,sizeof(s));
    /* Host is absent for calls 2 and 3, then sees the small final call 4. */
    A13DR_Accumulate(&s,100U,1U,0U);
    A13DR_Accumulate(&s,719999U,2U,1U);
    A13DR_Accumulate(&s,200U,3U,1U);
    A13DR_Accumulate(&s,150U,4U,1U);
    CHECK(s.max_cycles[1]==719999U && s.peak_sequence[1]==2U);
    CHECK(s.calls[1]==3U && s.last_sequence==4U);
    CHECK((s.global_peak_sequence_path & UINT32_C(0x0fffffff))==2U);
    CHECK((s.global_peak_sequence_path>>28U)==1U);
    CHECK(s.generation==8U);
    A13DR_Accumulate(&s,720001U,5U,4U);
    CHECK(s.max_cycles[4]==720001U && s.peak_sequence[4]==5U);
    A13DR_Accumulate(&s,10U,7U,4U);
    CHECK((s.stack_offset_flags & UINT16_C(0x2000))!=0U);
    (void)memset(&s,0,sizeof(s));
    s.calls[0]=UINT16_MAX;
    A13DR_Accumulate(&s,1U,1U,0U);
    CHECK(s.calls[0]==UINT16_MAX);
    CHECK((s.stack_offset_flags & UINT16_C(0x2000))!=0U);
    (void)memset(&s,0,sizeof(s));
    s.generation=UINT32_MAX-1U;
    A13DR_Accumulate(&s,1U,1U,0U);
    CHECK(s.generation==UINT32_MAX-1U);
    CHECK((s.stack_offset_flags & UINT16_C(0x2000))!=0U);
    CHECK(A13DR_Path(7U,true)==5U && A13DR_Path(7U,false)==4U);
    CHECK(A13DR_Path(8U,false)==9U);
    (void)puts("A13D-R target updater delay/peak/count/overflow tests PASS");
    return 0;
}
