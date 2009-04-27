#ifndef _ARMX86_DEBUG_H
#define _ARMX86_DEBUG_H

#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <string.h>

/*
 * Error and Message Handling:
 *
 * debug statements are meant for the developer(s) only. These can
 * only be produced in a debug build.
 *
 * info messages are verbose progress information for the user to
 * enable from the command line.
 *
 * panic-s are to terminate the program at a point of unrecoverable
 * errors. They are only meant to catch programming errors and not
 * input data inconsistencies (which should be handled via a more
 * elegant exit strategy). asserts are present in debug as well as
 * release builds.
 *
 * sys_err messages are used in the failure path for system calls.
 * They are available in debug as well as release builds, and 
 * append standard error string based on errno to user strings.
 *
 */

/* Information - formatted string in double brackets */
#define info(str)           printf str

/* Panic - boolean condition, formatted string in brackets */
#define err_print(str)      printf str, fflush(stdout)
#define panic(cond, str)    ((cond)?1:(err_print(str), assert(0)))

/* System Error messages - formatted string, no newline */
#define sys_err(str)        printf str; printf(": %s\n", strerror(errno));

/* Debug messages - formatted string in double brackets */
#define dbg_prefix          printf("%s (%d) [%s()]: ",          \
                                __FILE__, __LINE__, __FUNCTION__)
#ifdef DEBUG
#define debug(str)          dbg_prefix; printf str
#define debug_in            debug(("Enter\n"))
#define debug_out           debug(("Leave\n"))
#else /* DEBUG */
#define debug(str)          
#define debug_in            
#define debug_out           
#endif /* DEBUG */

/*                         OLD                 */

#define DP_INFO             printf("%s, %s(): ", __FILE__, __FUNCTION__)
#define DP_PREFIX           printf("(dbg) "),DP_INFO
#define print(x)            DP_PREFIX,printf(x)
#define print1(x,x1)        DP_PREFIX,printf((x),(x1))
#define print2(x,x1,x2)     DP_PREFIX,printf((x),(x1),(x2))
#define print3(x,x1,x2,x3)  DP_PREFIX,printf((x),(x1),(x2),(x3))

#define DP_ASSERT(x,y)      (x)?1:(print(y),assert(0));

#ifdef DEBUG

#define DP(x)               print(x)
#define DP1(x,x1)           print1((x),(x1))
#define DP2(x,x1,x2)        print2((x),(x1),(x2))
#define DP3(x,x1,x2,x3)     print3((x),(x1),(x2),(x3))
#define DP_HI               DP("Enter\n"); 
#define DP_BYE              DP("Leave\n"); 

#else /* DEBUG */

#define DP(x)
#define DP1(x,x1)
#define DP2(x,x1,x2)
#define DP3(x,x1,x2,x3)
#define DP_HI
#define DP_BYE

#endif /* DEBUG */

#endif /* _ARMX86_DEBUG_H */
