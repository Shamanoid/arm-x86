#ifndef _CODEGEN_H
#define _CODEGEN_H

typedef OPCODE_HANDLER_RETURN (*opcodeHandler_t)(void *inst);
extern opcodeHandler_t opcodeHandler[NUM_OPCODES];

extern void *nextBB;

extern uint32_t cpsr;     /* ARM Program Status Register for user mode */
extern uint32_t x86Flags; /* x86 Flag Register */

extern int32_t regFile[NUM_ARM_REGISTERS];

extern void callEndBBTaken();

#ifndef NOCHAINING

/*
 * These are a couple of variables used for chaining. When, at the end of a
 * basic block the taken or untaken callout is invoked, one of these variables
 * is populated at the call location to indicate to the handler where the call
 * has come from. This allows the handler to patch the call location to chain
 * the next basic block to it.
 */
void* pTakenCalloutSourceLoc;
void* pUntakenCalloutSourceLoc;
#endif /* NOCHAINING */

#ifdef DEBUG

#define LOG_INSTR(addr,count) { \
  int i;                        \
  printf("%p: ",addr);          \
  for(i=0; i<(count); i++){     \
    printf(" %x",*((addr)+i));  \
  }                             \
  printf("\n");                 \
}
#else
#define LOG_INSTR(addr,count)   
#endif

/*
// A couple of convenience macros to help insert code into the translation
// cache while keeping the code readable.
*/
#define ADD_BYTE(x)                                     \
  *((uint8_t *)instInfo.pX86Addr + count) = (x);        \
  count++;

#define ADD_WORD(x)                                     \
  *(uint32_t *)(instInfo.pX86Addr + count) = (x);       \
  count+=4;

#define DPREG_INFO              instInfo.armInstInfo.dpreg
#define DPIMM_INFO              instInfo.armInstInfo.dpimm
#define LSMULT_INFO             instInfo.armInstInfo.lsmult
#define LSREG_INFO              instInfo.armInstInfo.lsreg
#define LSIMM_INFO              instInfo.armInstInfo.lsimm
#define BRCH_INFO               instInfo.armInstInfo.branch


#endif /* _CODEGEN_H */
