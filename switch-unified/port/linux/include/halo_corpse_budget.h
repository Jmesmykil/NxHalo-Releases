#ifndef HALO_CORPSE_BUDGET_H
#define HALO_CORPSE_BUDGET_H
#define HALO_CORPSE_SOLVERS_PER_TICK 16
/* Stable population completes a full round in ceil(count/16) ticks. */
static unsigned halo_corpse_budget_select(const long *pending,unsigned count,long *selected,unsigned *cursor)
{
 unsigned n=count<HALO_CORPSE_SOLVERS_PER_TICK?count:HALO_CORPSE_SOLVERS_PER_TICK;
 if(!count){*cursor=0;return 0;}
 unsigned start=*cursor%count;
 for(unsigned i=0;i<n;i++)selected[i]=pending[(start+i)%count];
 *cursor=(start+n)%count;
 return n;
}
#endif
