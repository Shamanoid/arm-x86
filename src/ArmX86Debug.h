#ifndef _ARMX86_DEBUG_H
#define _ARMX86_DEBUG_H

#include <stdio.h>
#include <assert.h>

#define DP_INFO             printf("%s, %s(): ",__FILE__,__FUNCTION__)
#define DP_PREFIX           printf("(dbg) "),DP_INFO
#define print(x)            DP_PREFIX,printf(x)
#define print1(x,x1)        DP_PREFIX,printf((x),(x1))
#define print2(x,x1,x2)     DP_PREFIX,printf((x),(x1),(x2))
#define print3(x,x1,x2,x3)  DP_PREFIX,printf((x),(x1),(x2),(x3))

#define DP_ASSERT(x,y)      (x)?1:(print(y),assert(0));

#ifdef DEBUG

#define DP(x)               print(x)
#define DP1(x,x1)           print1((x),(x1))
#define DP_HI               DP("Enter\n"); 
#define DP_BYE              DP("Leave\n"); 

#else /* DEBUG */

#define DP(x)
#define DP1(x,x1)
#define DP_HI
#define DP_BYE

#endif /* DEBUG */

#endif /* _ARMX86_DEBUG_H */
