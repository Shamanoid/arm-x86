#ifndef _ARMX86_TYPES_H
#define _ARMX86_TYPES_H
 
#include <stdint.h>

struct map_t{
    uint32_t *pArmInstr;
    uint8_t *pX86Instr;
    uint32_t *pArmStackPtr;
};

typedef enum{
    FALSE = 0,
    TRUE = 1
} bool;
 
#endif /* _ARMX86_TYPES_H */
