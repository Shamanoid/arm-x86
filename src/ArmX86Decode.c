#include <stdint.h>
#include <stdlib.h>
#include "ArmX86Debug.h"
#include "ArmX86Decode.h"
#include "ArmX86Types.h"
#include "ArmX86DecodePrivate.h"
#include "ArmX86CodeGen.h"

int32_t regFile[NUM_ARM_REGISTERS] = {
  0x0, 0x1, 0x2, 0x3,
  0x4, 0x5, 0x6, 0x7,
  0x8, 0x9, 0xA, 0xB,
  0xC, 0xD, 0xE, 0xF
};

uint32_t cpsr;     /* ARM Program Status Register for user mode */
uint32_t x86Flags; /* x86 Flag Register */
void *nextBB;

#ifndef NOCHAINING
/*
// These are a couple of variable used for chaining. When, at the end of a
// basic block the taken or untaken callout is invoked, one of these variables
// is populated at the call location to indicate to the handler where the call
// has come from. This allows the handler to patch the call location to chain
// the next basic block to it.
*/
void* pTakenCalloutSourceLoc;
void* pUntakenCalloutSourceLoc;
#endif /* NOCHAINING */

typedef OPCODE_HANDLER_RETURN (*opcodeHandler_t)(void *inst);
opcodeHandler_t opcodeHandler[NUM_OPCODES] = {
  andHandler, eorHandler, subHandler, rsbHandler,
  addHandler, adcHandler, sbcHandler, rscHandler,
  tstHandler, teqHandler, cmpHandler, cmnHandler,
  orrHandler, movHandler, bicHandler, mvnHandler
};

#define DPREG_INFO              instInfo.armInstInfo.dpreg
#define DPIMM_INFO              instInfo.armInstInfo.dpimm
#define LSMULT_INFO             instInfo.armInstInfo.lsmult
#define LSREG_INFO              instInfo.armInstInfo.lsreg
#define LSIMM_INFO              instInfo.armInstInfo.lsimm
#define BRCH_INFO               instInfo.armInstInfo.branch

typedef void (*translator)(void);

#ifdef DEBUG

#if 0
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

#define DISPLAY_REGS {                           \
  int i;                                         \
  printf("=========\n");                         \
  for(i=0; i< 16; i++){                          \
    if(i>0 && ((i%4) == 0)){                     \
      printf("\n");                              \
    }                                            \
    printf("R[%2d] = 0x%08X ",i, regFile[i]);    \
  }                                              \
  printf("\n");                                  \
  printf("Flags = 0x%x\n",x86Flags);             \
  printf("=========\n");                         \
}                                                \

#else

#define LOG_INSTR(addr,count) 
#define DISPLAY_REGS 

#endif /* DEBUG */

/*
// Macros to help manage translation index.
*/
#define INDEXED_BLOCK(block) GetItem((block))
#define INDEX_BLOCK(armBlock,x86Block) InsertItem(armBlock,x86Block) 

uint32_t *pArmPC;
uint8_t *pX86PC;

void callEndBBTaken(){
  DP_HI;

  DISPLAY_REGS;
  /*
  // FIXME: How should a program end?
  */
  if(nextBB == NULL){
    printf("%d\n",regFile[0]);
    exit(0);
  }
  DP1("Next BB Address = %p\n",nextBB);

#ifndef NOCHAINING
  DP1("I am %p\n",&callEndBBTaken);
  DP1("Got here from address %p\n",pTakenCalloutSourceLoc);
  DP1("Offset from call location = 0x%x\n",
    (intptr_t)&callEndBBTaken - (intptr_t)pTakenCalloutSourceLoc);

  if(pTakenCalloutSourceLoc != 0x00000000){
    uint8_t *nextX86BB;
    DP2("Caller Dump: 0x%x 0x%x\n",
      *(uint8_t *)pTakenCalloutSourceLoc,
      *(uint32_t *)((uint8_t *)pTakenCalloutSourceLoc + 1)
    );

    if((nextX86BB = (uint8_t *)INDEXED_BLOCK((void *)nextBB)) == NULL){
      nextX86BB = pX86PC;
    }
    DP1("Chaining BB to %p\n",nextX86BB);
    *(uint8_t *)pTakenCalloutSourceLoc = X86_OP_JMP;
    *(uint32_t *)((uint8_t *)pTakenCalloutSourceLoc + 1) = 
      (uintptr_t)nextX86BB - 
        ((uintptr_t)((uint8_t *)pTakenCalloutSourceLoc + 5)
      );
  }
#endif /* NOCHAINING */

  pArmPC = nextBB;
  decodeBasicBlock();

  DP_BYE;
}

void callEndBBNotTaken(){
  DP_HI;

#ifndef NOCHAINING
  DP1("I am %p\n",&callEndBBNotTaken);
  DP1("Got here from address %p\n",pUntakenCalloutSourceLoc);
  DP1("Offset from call location = 0x%x\n",
    (intptr_t)&callEndBBNotTaken - (intptr_t)pUntakenCalloutSourceLoc);
  DP2("Caller Dump: 0x%x 0x%x\n",
    *(uint8_t *)pUntakenCalloutSourceLoc,
    *(uint32_t *)((uint8_t *)pUntakenCalloutSourceLoc + 1)
  );
  DP1("Next BB Address = %p\n",nextBB);

  uint8_t *nextX86BB;
  if((nextX86BB = (uint8_t *)INDEXED_BLOCK((void *)nextBB)) == NULL){
    nextX86BB = pX86PC;
  }
  DP1("Chaining BB to %p\n",nextX86BB);
  *(uint8_t *)pUntakenCalloutSourceLoc = X86_OP_JMP;
  *(uint32_t *)((uint8_t*)pUntakenCalloutSourceLoc + 1) = (uintptr_t)nextX86BB
    - ((uintptr_t)((uint8_t *)pUntakenCalloutSourceLoc + 5));
#endif /* NOCHAINING */

  DISPLAY_REGS;
  pArmPC = nextBB;
  decodeBasicBlock();

  DP_BYE;
}

/*
// A couple of convenience macros to help insert code into the translation
// cache while keeping the code readable.
*/
#define ADD_BYTE(x)                                     \
  *((uint8_t *)instInfo.pX86Addr + count) = (x);        \
  count++;                                              \

#define ADD_WORD(x)                                     \
  *(uint32_t *)(instInfo.pX86Addr + count) = (x);       \
  count+=4;                                             \

uint8_t handleConditional(void *pInst){ 
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  DP_HI;

  ADD_BYTE(X86_OP_PUSH_MEM32);
  ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
  ADD_WORD((uintptr_t)&x86Flags);
  ADD_BYTE(X86_OP_POPF);
  LOG_INSTR(instInfo.pX86Addr,count);

  DP1("cond = %d\n",instInfo.cond);
  switch(instInfo.cond){
    case COND_EQ:
      /* jne */
      ADD_BYTE(X86_PRE_JCC);
      ADD_BYTE(X86_OP_JNE);
    break;
    case COND_NE:
      /* je */
      ADD_BYTE(X86_PRE_JCC);
      ADD_BYTE(X86_OP_JE);
    break;
    case COND_CS:
      /* jnc */
      ADD_BYTE(X86_PRE_JCC);
      ADD_BYTE(X86_OP_JNC);
    break;
    case COND_CC:
      /* jc */
      ADD_BYTE(X86_PRE_JCC);
      ADD_BYTE(X86_OP_JC);
    break;
    case COND_MI:
    case COND_PL:
    case COND_VS:
    case COND_VC:
      DP_ASSERT(0,"Unimplemented flag check\n");
    break;
    case COND_HI:
      /* cmc + jna */
      ADD_BYTE(X86_OP_CMC);
      ADD_BYTE(X86_PRE_JCC);
      ADD_BYTE(X86_OP_JNA); 
    break;
    case COND_LS:
      /* cmc + jnbe */
      ADD_BYTE(X86_OP_CMC);
      ADD_BYTE(X86_PRE_JCC);
      ADD_BYTE(X86_OP_JNBE); 
    break;
    case COND_GE:
      /* jl */
      ADD_BYTE(X86_PRE_JCC);
      ADD_BYTE(X86_OP_JL);
    break;
    case COND_LT:
      /* jnl or jge */
      ADD_BYTE(X86_PRE_JCC);
      ADD_BYTE(X86_OP_JNL);
    break;
    case COND_GT:
      /* jng or jle */
      ADD_BYTE(X86_PRE_JCC);
      ADD_BYTE(X86_OP_JNG);
    break;
    case COND_LE:
      /* jg */
      ADD_BYTE(X86_PRE_JCC)
      ADD_BYTE(X86_OP_JG)
    break;
    case COND_UNDEF:
      DP_ASSERT(0, "Unsupported condition code\n");
    default:
      DP_ASSERT(0, "Invalid condition code\n");
    break;
  }

  DP_BYE;

  return count;
}

#ifdef DEBUG
/*
// Include a couple of profiling variables. These are not really
// indicative because a static count of instructions that modify
// flags is not as important as the run-count for instructions
// that do. But keep them around anyway.
*/
uint32_t sEq1Count = 0;
uint32_t sEq0Count = 0;

#endif /* DEBUG */

translator x86Translator;

void armX86Decode(const struct map_t *memMap){
  pArmPC = memMap->pArmInstr;
  pX86PC = memMap->pX86Instr;
  x86Translator = (translator)memMap->pX86Instr;

  /*
  // FIXME: This is a temporary initialization of the stack pointer
  // to some place that the os is happy letting me access.
  */
  PC = (uintptr_t)memMap->pArmInstr;
  SP = (uintptr_t)memMap->pArmStackPtr;
  LR = 0;

  decodeBasicBlock();
}

void decodeBasicBlock(){
  struct decodeInfo_t instInfo;
  uint32_t armInst;

  /*
  // x86InstCount is number of bytes of x86 instructions
  */
  uint32_t x86InstCount;
  uint8_t *pCondJumpOffsetAddr = 0;
  uint8_t count = 0;

  DP_HI;

  DP1("x86PC = %p\n",pX86PC);
  DP1("armPC = %p\n",pArmPC);

  if((x86Translator = (translator)INDEXED_BLOCK((void *)pArmPC)) != NULL){
    instInfo.endBB = TRUE;
 
    DP("Found previously translated block\n");
    DP2("ARM %p -> %p\n",pArmPC,x86Translator);
  }else{
    DP1("Untranslated basic block at %p\n",pArmPC);

#ifndef NOINDEX
    INDEX_BLOCK((void *)pArmPC,(void *)pX86PC);
#endif /* NOINDEX */

    instInfo.endBB = FALSE;
    x86Translator = (translator)pX86PC;

    while(instInfo.endBB == FALSE){
      DP2("Processing instruction: 0x%x @ %p\n",*pArmPC, (void *)pArmPC);
      DP1("x86PC = %p\n",pX86PC);
      count = 0;
      armInst = *pArmPC;
      instInfo.pArmAddr = pArmPC;

      /*
      // First check the condition field. Set a jump in the code if the
      //  instruction is to be executed conditionally.
      */
      instInfo.cond = ((armInst & COND_MASK) >> COND_SHIFT);
      instInfo.pX86Addr = pX86PC;
      if(instInfo.cond != AL){
        x86InstCount = handleConditional((void *)&instInfo);
        pCondJumpOffsetAddr = instInfo.pX86Addr + x86InstCount;
        x86InstCount += 4; /* Reserve space for a 4-byte offset */
        *(uint32_t *)pCondJumpOffsetAddr = 0; /* Set the offset to 0 at first */
        pX86PC += x86InstCount; 
      }

      /*
      // There are usually two source operands and one destination register for
      // data processing instruction. The compare, test and move instructions
      // are exceptions. Some of them have only one source operand and other
      // do not have a destination register.
      */

      /*
      // Instructions are decoded by family
      */
      switch(armInst & INST_TYPE_MASK){
        case INST_TYPE_DP_MISC:
          /*
          // There are two conditions that help distinguish an instruction
          // as a data processing instruction:
          //   Bit 4 and Bit 7 should not both be '1'
          //   In case the top two bits of the opcode are "10", the S bit
          //   should be a '1'
          */
          if(((armInst & 0x01900000) != 0x01000000) && 
            ((armInst & 0x00000090) != 0x00000090)){
            DPREG_INFO.Rn = RN(armInst);
            DPREG_INFO.Rm = RM(armInst);
            DPREG_INFO.Rd = RD(armInst);
            DPREG_INFO.S = ((armInst & BIT20_MASK) > 0?TRUE:FALSE);

#ifdef DEBUG
            if(DPREG_INFO.S == TRUE){
              sEq1Count++;
            }else{
              sEq0Count++;
            }
#endif /* DEBUG */

            instInfo.pX86Addr = pX86PC;
            instInfo.immediate = FALSE;
            DPREG_INFO.shiftType = 
              ((armInst & SHIFT_TYPE_MASK) >> SHIFT_TYPE_SHIFT);
            if((armInst & 0x00000010) == 0){
              DPREG_INFO.shiftAmt = 
                ((armInst & SHIFT_AMT_MASK) >> SHIFT_AMT_SHIFT);
              DPREG_INFO.shiftImm = TRUE;
            }else{
              DPREG_INFO.Rs = RS(armInst);
              DPREG_INFO.shiftImm = FALSE;
            }
            x86InstCount = 
              (opcodeHandler[((armInst & OPCODE_MASK) >> OPCODE_SHIFT)])
              ((void *)&instInfo);
            pX86PC += x86InstCount;
          }else{
            UNSUPPORTED;
          }
          instInfo.pX86Addr = pX86PC;

          /*
          // The mov instruction may have set the PC to a new value. Handle this
          // as the end of a basic block.
          */
          if(instInfo.endBB == TRUE){
            if(instInfo.cond != AL){
              count = 0;
              ADD_BYTE(X86_OP_MOV_IMM_TO_MEM32);
              ADD_BYTE(0x05); /* MOD R/M for mov imm32 to rm32 0xC7 /0 */
              ADD_WORD((uintptr_t)&nextBB);
              ADD_WORD((uint32_t)((uintptr_t)pArmPC + 4));

#ifndef NOCHAINING
              ADD_BYTE(X86_OP_MOV_IMM_TO_MEM32);
              ADD_BYTE(0x05); /* MOD R/M for mov imm32 to rm32 0xC7 /0 */
              ADD_WORD((uintptr_t)&pUntakenCalloutSourceLoc);
              ADD_WORD((uintptr_t)(pX86PC + count + 4));
#endif /* NOCHAINING */

              ADD_BYTE(X86_OP_CALL);
              ADD_WORD((uintptr_t)(
                (intptr_t)&callEndBBNotTaken - (intptr_t)(pX86PC + count + 4)
              ));
              pX86PC += count;
            }
          }
          instInfo.pX86Addr = pX86PC;
        break;
        case INST_TYPE_IMM_UNDEF:
          if((armInst & 0x01900000) != 0x01000000){
            DPIMM_INFO.Rn = RN(armInst);
            DPIMM_INFO.Rd = RD(armInst);
            DPIMM_INFO.rotate = ROTATE(armInst);
            DPIMM_INFO.imm = (armInst & 0x000000FF);
            DPIMM_INFO.S = ((armInst & BIT20_MASK) >0?TRUE:FALSE);

#ifdef DEBUG
            if(DPREG_INFO.S == TRUE){
              sEq1Count++;
            }else{
              sEq0Count++;
            }
#endif /* DEBUG */

            instInfo.pX86Addr = pX86PC;
            instInfo.immediate = TRUE;
            x86InstCount = 
              (opcodeHandler[((armInst & OPCODE_MASK) >> OPCODE_SHIFT)])
              ((void *)&instInfo);
            pX86PC += x86InstCount;
          }else{
            UNSUPPORTED;
          }
        break;
        case INST_TYPE_LSIMM:
          LSIMM_INFO.P = ((armInst & BIT24_MASK) > 0?TRUE:FALSE);
          LSIMM_INFO.U = ((armInst & BIT23_MASK) > 0?TRUE:FALSE);
          LSIMM_INFO.B = ((armInst & BIT22_MASK) > 0?TRUE:FALSE);
          LSIMM_INFO.W = ((armInst & BIT21_MASK) > 0?TRUE:FALSE);
          LSIMM_INFO.L = ((armInst & BIT20_MASK) > 0?TRUE:FALSE);
          LSIMM_INFO.Rn = RN(armInst);
          LSIMM_INFO.Rd = RD(armInst);
          LSIMM_INFO.imm = (armInst & 0x00000FFF);
          instInfo.pX86Addr = pX86PC;
          x86InstCount = lsimmHandler((void *)&instInfo);
          pX86PC += x86InstCount;
        break;
        case INST_TYPE_LSR_UNDEF:
          if((armInst & 0x00000010) == 0){
            LSREG_INFO.P = ((armInst & BIT24_MASK) > 0?TRUE:FALSE);
            LSREG_INFO.U = ((armInst & BIT23_MASK) > 0?TRUE:FALSE);
            LSREG_INFO.B = ((armInst & BIT22_MASK) > 0?TRUE:FALSE);
            LSREG_INFO.W = ((armInst & BIT21_MASK) > 0?TRUE:FALSE);
            LSREG_INFO.L = ((armInst & BIT20_MASK) > 0?TRUE:FALSE);
            LSREG_INFO.Rn = RN(armInst);
            LSREG_INFO.Rm = RM(armInst);
            LSREG_INFO.Rd = RD(armInst);
            LSREG_INFO.shiftAmt = 
              ((armInst & SHIFT_AMT_MASK) >> SHIFT_AMT_SHIFT);
            LSREG_INFO.shiftType = 
              ((armInst & SHIFT_TYPE_MASK) >> SHIFT_TYPE_SHIFT);
            instInfo.pX86Addr = pX86PC;
            x86InstCount = lsregHandler((void *)&instInfo);
            pX86PC += x86InstCount;
          }else{
            UNSUPPORTED;
          }
        break;
        case INST_TYPE_LSMULT:
          LSMULT_INFO.Rn = RN(armInst);
          LSMULT_INFO.regList = armInst & 0x0000FFFF;
          LSMULT_INFO.P = ((armInst & BIT24_MASK) > 0?TRUE:FALSE);
          LSMULT_INFO.U = ((armInst & BIT23_MASK) > 0?TRUE:FALSE);
          LSMULT_INFO.S = ((armInst & BIT22_MASK) > 0?TRUE:FALSE);
          LSMULT_INFO.W = ((armInst & BIT21_MASK) > 0?TRUE:FALSE);
          LSMULT_INFO.L = ((armInst & BIT20_MASK) > 0?TRUE:FALSE);
          instInfo.pX86Addr = pX86PC;
          x86InstCount = lsmHandler((void *)&instInfo);
          pX86PC += x86InstCount;
          instInfo.pX86Addr = pX86PC;

          /*
          // The load instruction has set the PC to a new value. Handle this
          // as the end of a basic block.
          */
          if(instInfo.endBB == TRUE){
            count = 0;

            ADD_BYTE(X86_OP_PUSH_MEM32);
            ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
            ADD_WORD((uintptr_t)&regFile[15]);
            ADD_BYTE(X86_OP_POP_MEM32);
            ADD_BYTE(0x05); /* MOD R/M for POP - 0x8F /0 */
            ADD_WORD((uintptr_t)&nextBB);

#ifndef NOCHAINING
            ADD_BYTE(X86_OP_MOV_IMM_TO_MEM32);
            ADD_BYTE(0x05); /* MOD R/M for mov imm32 to rm32 0xC7 /0 */
            ADD_WORD((uintptr_t)&pTakenCalloutSourceLoc);
            ADD_WORD((uintptr_t)(pX86PC + count + 4));
#endif /* NOCHAINING */

            pX86PC += count;

            ADD_BYTE(X86_OP_CALL);
            ADD_WORD((uintptr_t)(
              (intptr_t)&callEndBBTaken - (intptr_t)(pX86PC + 5)
            ));
            pX86PC += 5;
          }
        break;
        case INST_TYPE_BRCH:
          BRCH_INFO.L = ((armInst & BIT24_MASK) > 0?TRUE:FALSE);
          BRCH_INFO.offset = (armInst & OFFSET_MASK);
          instInfo.pX86Addr = pX86PC;
          x86InstCount = brchHandler((void *)&instInfo);
          pX86PC += x86InstCount;
          instInfo.pX86Addr = pX86PC;

          /*
          // There is a little trick here. A conditional branch may be thought
          // of as a branch instruction that is executed when the condition is
          // True. So a conditional branch is treated like all other conditional
          // instructions. However, for all other instructions, it suffices to
          // jump beyond the instruction. However, in the case of the branch
          // jumping beyond the instruction should equate to a branch that is 
          // untaken. So at the instruction 'beyond', place a call to the 
          // handler for the NotTakenBranch. The offset for the conditional
          //  jump points to this call instrution.
          */
          if(instInfo.cond != AL){
            count = 0;
            ADD_BYTE(X86_OP_MOV_IMM_TO_MEM32);
            ADD_BYTE(0x05); /* MOD R/M for mov imm32 to rm32 0xC7 /0 */
            ADD_WORD((uintptr_t)&nextBB);
            ADD_WORD((uint32_t)((uintptr_t)pArmPC + 4));

#ifndef NOCHAINING
            ADD_BYTE(X86_OP_MOV_IMM_TO_MEM32);
            ADD_BYTE(0x05); /* MOD R/M for mov imm32 to rm32 0xC7 /0 */
            ADD_WORD((uintptr_t)&pUntakenCalloutSourceLoc);
            ADD_WORD((uintptr_t)(pX86PC + count + 4));
#endif /* NOCHAINING */

            ADD_BYTE(X86_OP_CALL);
            ADD_WORD((uintptr_t)(
              (intptr_t)&callEndBBNotTaken - (intptr_t)(pX86PC + count + 4)
            ));
            pX86PC += count;
          }
        break;
        case INST_TYPE_COPLS:
          UNSUPPORTED;
        break;
        case INST_TYPE_COP_SWI:
          x86InstCount = 0;
          pX86PC += x86InstCount;
          instInfo.pX86Addr = pX86PC;
          DP("************* IGNORING Software Interrupt ****************\n");
        break;
        default:
          UNSUPPORTED;
        break;
      }
      if(instInfo.cond != AL){
        *(uint32_t *)pCondJumpOffsetAddr = x86InstCount;
      }

      pArmPC++;
    }
  
    DP1("x86PC = %p\n",pX86PC);
  }
  DISPLAY_REGS;

  asm ("jmp *x86Translator");

  DISPLAY_REGS;

#ifdef DEBUG
  float sEq1Percent = ((100.0 * sEq1Count)/(sEq1Count + sEq0Count));
  float sEq0Percent = ((100.0 * sEq0Count)/(sEq1Count + sEq0Count));

  DP1("%.2f percent instructions modify flags\n",sEq1Percent);
  DP1("%.2f percent instructions don't modify flags\n",sEq0Percent);
#endif /* DEBUG */

  DP_BYE;
}

/*
// Strategy for condition evaluation
//
// There are instructions in ARM that modify flags some of the time but not
// at other times. In the x86 instruction set, some instructions modify flags
// all the time and some never do. This subtle difference is the philosophy
// of the two instruction sets offers an interesting challenge to converting
// code from one instruction set to the other.
//
// That a particular ARM instruction will update a flag is indicated by a bit
// named 'S' that is present in every data processing instruction. If the 'S'
// bit is '1', the flags will be updated, and if the 'S' bit is '0', they will
// not be. Instruction such as 'compare', whose sole purpose is to update flags
// are not permitted to have 'S' set to '0'.
//
// The challenge of conversion is interesting because in ARM programs, those
// instructions that update flags can be freely interspersed with other
// instructions that would normally update flags but are configured by the 
// compiler not to do so. For instance, an 'add' should update the carry, but
// the compiler might choose it not to. This might be because the compiler
// finds that the result does not influence a decision in any case. In contrast,
// an 'add' performed on x86 would unconditionally update the carry, and
// possibly the sign and overflow flags. Clearly, a one-to-one translation
// would break the semantics of the program. Consider the case where the
// sequence of ARM instructions is as follows:
//   sub R10, R10, R10 (encoded with S = 1)
//   add R2, R1, R0 (encoded with S = 0)
//   beq <addr> (branch if 0)
// Here the sub instruction changes the zero flag. The add that follows it
// normally would but does not in this case because 'S' is cleared in the
// encoding of the instruction. The beq instruction uses the result of the
// subtraction.
// A direct translation of the instructions to x86 would make the x86 condition
// check use the result of the add!
//
// It is observed that the trend in the gcc compiler for ARM is that unless
// an instruction produces a result that influences a conditional branch
// or conditional expression, the 'S' bit in the instruction is set to 0.
// This means that only instructions that modify flags for a if-then-else
// or instructions that modify the loop control variable in the a loop would
// have the 'S' bit set to 1. Assuming that a loop has a few instructions,
// perhaps only one instruction close to the end of the loop would modify the
// flag that controls the loop. Obviously, the ratio os instructions that
// modify the flags to those that do not, is very small. It follows that
// the strategy that offers greater speed would be one that treats instructions
// that do modify flags as special cases.
//
// Hence the following strategy is adopted:
// There is a flags variable that is maintained as a global variable by the
// binary translator and meant to be a snapshot of the x86 flags. When an
// instruction has the 'S' bit set to 1, this variable is loaded into the x86
// flags register using the following combination of instructions:
//   push <flags> Pushes the global flags variable on to the stack
//   popf         Loads the flags register from the top of the stack
// This is like loading the last flag context, i.e. the context created by the
// last instruction to modify the flags.
// An instruction that checks the condition and executes can then proceed
// by using native x86 condition checks. An instruction that modifies the
// flags can update the x86 flags register itself.
// Once the instruction has been completed, if the flags have been updated,
// the snapshot of the flags must once again be saved. This is done by the
// following combination of instructions:
//   pushf        Pushes the flags register to the stack
//   pop <flags>  Loads the flags variable from the top of the stack
// This strategy ensures that even if there are instructions that do not
// modify flags that are placed in between instructions that do and those
// that check them (such a strategy may be adopted for optimization when the
// compiler wants to keep the pipeline full), the logic remains valid.
*/
/*
// FIXME: Don't bother making a copy of the entire data structure. Use
// pInst directly.
*/

int lsmHandler(void *pInst){
  struct decodeInfo_t instInfo
    = *(struct decodeInfo_t *)pInst;
  uint8_t count = 0;
  int8_t i, istart, iend, idelta;
  uint32_t disp = 0;

  DP("Load-Store Multiple\n");

  istart = ((LSMULT_INFO.U == 1)?0:(NUM_ARM_REGISTERS - 1));
  iend = ((LSMULT_INFO.U == 1)?NUM_ARM_REGISTERS:-1);
  idelta = ((LSMULT_INFO.U == 1)?1:-1);

  ADD_BYTE(X86_OP_MOV_TO_REG);
  ADD_BYTE(0x15) /* MODR/M - Mov from disp32 to edx */
  ADD_WORD((uintptr_t)&regFile[LSMULT_INFO.Rn]);

  for(i=istart; i != iend; i += idelta){
    if((LSMULT_INFO.regList & (0x00000001 << i)) == 0){
      continue;
    }
    if(LSMULT_INFO.L == 0){ /* Store */
      /*
      // If P = 1, the base register needs to be excluded. So increment early.
      */
      if(LSMULT_INFO.P != 0){
        disp+=4;
      }
      ADD_BYTE(X86_OP_MOV_TO_EAX);
      ADD_WORD((uintptr_t)&regFile[i]);

      ADD_BYTE(X86_OP_MOV_FROM_REG);
      ADD_BYTE(0x82) /* MODR/M - Mov from eax to  edx + disp32 */
      ADD_WORD((int32_t)disp * (LSMULT_INFO.U == 0?-1:1));
      LOG_INSTR(instInfo.pX86Addr,count);

      /*
      // If P = 0, the base register needs to be included. So increment late.
      */
      if(LSMULT_INFO.P == 0){
        disp+=4;
      }
    }else{ /* Load */
      /*
      // If P = 1, the base register needs to be excluded. So increment early.
      */
      if(LSMULT_INFO.P != 0){
        disp+=4;
      }

      ADD_BYTE(X86_OP_MOV_TO_REG);
      ADD_BYTE(0x82) /* MODR/M - Mov from edx + disp32 to eax */
      ADD_WORD((int32_t)disp * (LSMULT_INFO.U == 0?-1:1));

      ADD_BYTE(X86_OP_MOV_FROM_EAX);
      ADD_WORD((uintptr_t)&regFile[i]);
      LOG_INSTR(instInfo.pX86Addr,count);
      
      /*
      // If P = 0, the base register needs to be included. So increment late.
      */
      if(LSMULT_INFO.P == 0){
        disp+=4;
      }

      /*
      // Finally, if the register is the PC, signal that this is the end of
      // a basic block.
      */
      if(i == 15){
        ((struct decodeInfo_t *)pInst)->endBB = TRUE;
      }
    }
  }

  /*
  // If the base register needs to be updated with the offset, do that now.
  */
  if(LSMULT_INFO.W == 1){
    ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
    ADD_WORD((int32_t)disp *(LSMULT_INFO.U == 0?-1:1))

    ADD_BYTE(X86_OP_ADD_REG_TO_REG);
    ADD_BYTE(0xC2); /* EDX and EAX, EAX is the destination */

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[LSMULT_INFO.Rn]);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  if(((struct decodeInfo_t *)pInst)->endBB == TRUE){
    ADD_BYTE(X86_OP_PUSH_MEM32);
    ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&PC);
    ADD_BYTE(X86_OP_POP_MEM32);
    ADD_BYTE(0x05); /* MOD R/M for POP - 0x8F /0 */
    ADD_WORD((uintptr_t)&nextBB);

#ifndef NOCHAINING
    /*
    // Load from register cannot be chained.
    */
    ADD_BYTE(X86_OP_MOV_IMM_TO_MEM32);
    ADD_BYTE(0x05); /* MOD R/M for mov imm32 to rm32 0xC7 /0 */
    ADD_WORD((uintptr_t)&pTakenCalloutSourceLoc);
    ADD_WORD(0x00000000);
#endif /* NOCHAINING */

    ADD_BYTE(X86_OP_CALL);
    ADD_WORD((uintptr_t)(
      (intptr_t)&callEndBBTaken - (intptr_t)(instInfo.pX86Addr + count + 4)
    ));
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  return count;
}

int lsimmHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t *)pInst;
  uint8_t count = 0;

  DP("\tLoad-Store Immediate\n");

  DP_ASSERT(LSIMM_INFO.B != 1, "Byte transfer not supported\n");

  if(LSIMM_INFO.L == 1){
    DP2("Load: Rd = %d, Rn = %d\n",LSIMM_INFO.Rd, LSIMM_INFO.Rn);

    /*
    // If Rn is the PC, the addressing is PC relative. The PC should be
    // updated to where the ARM PC is at the moment. If the code were 
    // running on ARM hardware, R15 would be updated by hardware with 
    // each instruction. We need to update it manually here.
    // Instead of doing the update every instruction, update only when PC is
    // used.
    */
    if(LSIMM_INFO.Rn == 15){
      DP1("PC Relative Instruction. Updating PC to 0x%x\n",
        (uint32_t)((uint8_t *)pArmPC + 8)
      );

      ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
      ADD_WORD((uint32_t)((uint8_t *)pArmPC + 8));

      ADD_BYTE(X86_OP_MOV_FROM_EAX);
      ADD_WORD((uintptr_t)&regFile[15]);
      LOG_INSTR(instInfo.pX86Addr,count);
    }

    ADD_BYTE(X86_OP_MOV_TO_REG);
    ADD_BYTE(0x15) /* MODR/M - Mov from disp32 to edx */
    ADD_WORD((uintptr_t)&regFile[LSIMM_INFO.Rn]);

    ADD_BYTE(X86_OP_MOV_TO_REG);
    ADD_BYTE(0x82) /* MODR/M - Mov from edx + disp32 to eax */

    if(LSIMM_INFO.P == 0){
      /*
      // Indexing is post addressed - the base address is used to address
      // memory and updated by applying the offset later.
      */
      ADD_WORD(0x00000000);
    }else{
      ADD_WORD((int32_t)LSIMM_INFO.imm * (LSIMM_INFO.U == 0?-1:1));
    }

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[LSIMM_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    /*
    // If the base register needs to be updated with the offset, do that now.
    */
    if(LSIMM_INFO.P == 0 || LSIMM_INFO.W == 1){
      ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
      ADD_WORD((int32_t)LSIMM_INFO.imm * (LSIMM_INFO.U == 0?-1:1))

      ADD_BYTE(X86_OP_ADD_REG_TO_REG);
      ADD_BYTE(0xC2); /* EDX and EAX, EAX is the destination */

      ADD_BYTE(X86_OP_MOV_FROM_EAX);
      ADD_WORD((uintptr_t)&regFile[LSIMM_INFO.Rn]);
      LOG_INSTR(instInfo.pX86Addr,count);
    }

    /*
    // If the destination is the PC, this is the end of a basic block
    */
    if(LSIMM_INFO.Rd == 15){
      ((struct decodeInfo_t *)pInst)->endBB = TRUE;

      /*
      // FIXME: This can be optimized.
      */
      ADD_BYTE(X86_OP_PUSH_MEM32);
      ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
      ADD_WORD((uintptr_t)&PC);
      ADD_BYTE(X86_OP_POP_MEM32);
      ADD_BYTE(0x05); /* MOD R/M for POP - 0x8F /0 */
      ADD_WORD((uintptr_t)&nextBB);

#ifndef NOCHAINING
      ADD_BYTE(X86_OP_MOV_IMM_TO_MEM32);
      ADD_BYTE(0x05); /* MOD R/M for mov imm32 to rm32 0xC7 /0 */
      ADD_WORD((uintptr_t)&pTakenCalloutSourceLoc);
      ADD_WORD((uintptr_t)(instInfo.pX86Addr + count + 4));
#endif /* NOCHAINING */

      ADD_BYTE(X86_OP_CALL);
      ADD_WORD((uintptr_t)(
        (intptr_t)&callEndBBTaken - (intptr_t)(instInfo.pX86Addr + count + 4)
      ));
      LOG_INSTR(instInfo.pX86Addr,count);
    }
  }else{
    DP2("Store: Rd = %d, Rn = %d\n",LSIMM_INFO.Rd, LSIMM_INFO.Rn);

    ADD_BYTE(X86_OP_MOV_TO_REG);
    ADD_BYTE(0x15) /* MODR/M - Mov from disp32 to edx */
    ADD_WORD((uintptr_t)&regFile[LSIMM_INFO.Rn]);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[LSIMM_INFO.Rd]);

    ADD_BYTE(X86_OP_MOV_FROM_REG);
    ADD_BYTE(0x82) /* MODR/M - Mov from eax to  edx + disp32 */
    if(LSIMM_INFO.P == 0){
      /*
      // Indexing is post addressed - the base address is used to address
      // memory and updated by applying the offset later.
      */
      ADD_WORD(0x00000000);
    }else{
      ADD_WORD((int32_t)LSIMM_INFO.imm * (LSIMM_INFO.U == 0?-1:1));
    }

    /*
    // If the base register needs to be updated with the offset, do that now.
    */
    if(LSIMM_INFO.P == 0 || LSIMM_INFO.W == 1){
      ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
      ADD_WORD((int32_t)LSIMM_INFO.imm * (LSIMM_INFO.U == 0?-1:1))

      ADD_BYTE(X86_OP_ADD_REG_TO_REG);
      ADD_BYTE(0xC2); /* EDX and EAX, EAX is the destination */

      ADD_BYTE(X86_OP_MOV_FROM_EAX);
      ADD_WORD((uintptr_t)&regFile[LSIMM_INFO.Rn]);
      LOG_INSTR(instInfo.pX86Addr,count);
    }
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  return count;
}

int lsregHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t *)pInst;
  uint8_t count = 0;

  DP_ASSERT(LSREG_INFO.B != 1, "Byte transfer not supported\n");
  DP_ASSERT(LSREG_INFO.P != 0, "Post-Indexed addressing not supported\n");
  DP_ASSERT(LSREG_INFO.W != 1, "Pre-Indexed addressing not supported\n");

  if(LSREG_INFO.L == 1){
    DP3("Load: Rd = %d, Rn = %d, Rm = %d\n",
      LSREG_INFO.Rd,
      LSREG_INFO.Rn,
      LSREG_INFO.Rm
    );

    /*
    // If Rn is the PC, the addressing is PC relative. The PC should be
    // updated to where the ARM PC is at the moment. If the code were 
    // running on ARM hardware, R15 would be updated by hardware with 
    // each instruction. We need to update it manually here.
    // Instead of doing the update every instruction, update only when PC is
    // used.
    */
    if(LSREG_INFO.Rn == 15){
      DP1("PC Relative Instruction. Updating PC to 0x%x\n",
        (uint32_t)((uint8_t *)pArmPC + 8)
      );

      ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
      ADD_WORD((uint32_t)((uint8_t *)pArmPC + 8));

      ADD_BYTE(X86_OP_MOV_FROM_EAX);
      ADD_WORD((uintptr_t)&regFile[15]);
      LOG_INSTR(instInfo.pX86Addr,count);
    }

    ADD_BYTE(X86_OP_MOV_TO_REG);
    ADD_BYTE(0x15) /* MODR/M - Mov from disp32 to edx */
    ADD_WORD((uintptr_t)&regFile[LSREG_INFO.Rn]);

    if(LSREG_INFO.U == 1){
      ADD_BYTE(X86_OP_ADD_MEM32_TO_REG);
      ADD_BYTE(0x15); /* MODR/M */
      ADD_WORD((uintptr_t)&regFile[LSREG_INFO.Rm]);
    }else{
      ADD_BYTE(X86_OP_SUB_MEM32_FROM_REG);
      ADD_BYTE(0x15); /* MODR/M */
      ADD_WORD((uintptr_t)&regFile[LSREG_INFO.Rm]);
    }

    ADD_BYTE(X86_OP_MOV_TO_REG);
    ADD_BYTE(0x82) /* MODR/M - Mov from edx + disp32 to eax */
    ADD_WORD(0x00000000);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[LSREG_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    /*
    // If the destination is the PC, this is the end of a basic block
    */
    if(LSREG_INFO.Rd == 15){
      ((struct decodeInfo_t *)pInst)->endBB = TRUE;

      /*
      // FIXME: This can be optimized.
      */
      ADD_BYTE(X86_OP_PUSH_MEM32);
      ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
      ADD_WORD((uintptr_t)&PC);
      ADD_BYTE(X86_OP_POP_MEM32);
      ADD_BYTE(0x05); /* MOD R/M for POP - 0x8F /0 */
      ADD_WORD((uintptr_t)&nextBB);

#ifndef NOCHAINING
      ADD_BYTE(X86_OP_MOV_IMM_TO_MEM32);
      ADD_BYTE(0x05); /* MOD R/M for mov imm32 to rm32 0xC7 /0 */
      ADD_WORD((uintptr_t)&pTakenCalloutSourceLoc);
      ADD_WORD((uintptr_t)(instInfo.pX86Addr + count + 4));
#endif /* NOCHAINING */

      ADD_BYTE(X86_OP_CALL);
      ADD_WORD((uintptr_t)(
        (intptr_t)&callEndBBTaken - (intptr_t)(instInfo.pX86Addr + count + 4)
      ));
      LOG_INSTR(instInfo.pX86Addr,count);
    }
    if(LSREG_INFO.shiftAmt != 0){ 
      UNSUPPORTED;
    }
  }else{
    DP2("Store: Rd = %d, Rn = %d\n",LSIMM_INFO.Rd, LSIMM_INFO.Rn);

    ADD_BYTE(X86_OP_MOV_TO_REG);
    ADD_BYTE(0x15) /* MODR/M - Mov from disp32 to edx */
    ADD_WORD((uintptr_t)&regFile[LSREG_INFO.Rm]);

    if(LSREG_INFO.shiftAmt != 0){
      if(LSREG_INFO.shiftType == LSL){
        ADD_BYTE(X86_OP_SHL);
        ADD_BYTE(0xE2); /* MOD R/M edx /4 */
        ADD_BYTE(LSREG_INFO.shiftAmt);
      }else{
        UNSUPPORTED;
      }
    }

    if(LSREG_INFO.U == 1){
      ADD_BYTE(X86_OP_ADD_MEM32_TO_REG);
      ADD_BYTE(0x15); /* MODR/M */
      ADD_WORD((uintptr_t)&regFile[LSREG_INFO.Rn]);
    }else{
      ADD_BYTE(X86_OP_SUB_MEM32_FROM_REG);
      ADD_BYTE(0x15); /* MODR/M */
      ADD_WORD((uintptr_t)&regFile[LSREG_INFO.Rn]);
    }

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[LSIMM_INFO.Rd]);

    ADD_BYTE(X86_OP_MOV_FROM_REG);
    ADD_BYTE(0x82) /* MODR/M - Mov from eax to  edx + disp32 */
    ADD_WORD(0x00000000);
    LOG_INSTR(instInfo.pX86Addr,count);

  }

  return count;
}

int brchHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t *)pInst;
  uint8_t count = 0;

  DP("Branch\n");

  if(BRCH_INFO.L == TRUE){
    DP("Branch and Link Instruction\n");
    DP1("Link Address = 0x%x\n",(uintptr_t)instInfo.pArmAddr + 4);

    ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
    ADD_WORD((uintptr_t) instInfo.pArmAddr + 4);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&LR);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  int32_t branchOffset = BRCH_INFO.offset;
  branchOffset |= (((BRCH_INFO.offset & 0x00800000) > 0)?0xFF000000:0x00000000);
  branchOffset = (uint32_t)((branchOffset << 2) + 
                            (uintptr_t)instInfo.pArmAddr + 8);

  DP1("Branch Address = 0x%x\n",branchOffset);

  ADD_BYTE(X86_OP_MOV_IMM_TO_MEM32);
  ADD_BYTE(0x05); /* MOD R/M for mov imm32 to rm32 0xC7 /0 */
  ADD_WORD((uintptr_t)&nextBB);
  ADD_WORD(branchOffset);

#ifndef NOCHAINING
  ADD_BYTE(X86_OP_MOV_IMM_TO_MEM32);
  ADD_BYTE(0x05); /* MOD R/M for mov imm32 to rm32 0xC7 /0 */
  ADD_WORD((uintptr_t)&pTakenCalloutSourceLoc);
  ADD_WORD((uintptr_t)(instInfo.pX86Addr + count + 4));
#endif /* NOCHAINING */

  ADD_BYTE(X86_OP_CALL);
  ADD_WORD((uintptr_t)(
    (intptr_t)&callEndBBTaken - (intptr_t)(instInfo.pX86Addr + count + 4)
  ));

  ((struct decodeInfo_t *)pInst)->endBB = TRUE;

  return count;
}

OPCODE_HANDLER_RETURN
andHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;
  
  /*
  // FIXME: This check should be done differently depending on whether
  // the instruction is a register instruction or an immediate instruction.
  */
  if(DPREG_INFO.S == TRUE){
    DP("Updating flags\n");

    ADD_BYTE(X86_OP_PUSH_MEM32);
    ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    ADD_BYTE(X86_OP_POPF);
    LOG_INSTR(instInfo.pX86Addr,count);
    DP("Loading Flags\n");
  }

  if(instInfo.immediate == FALSE){
    DP2("Register: RN = %d\nRD = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);

    ADD_BYTE(X86_OP_AND_MEM32_TO_EAX);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rm]);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPREG_INFO.shiftAmt != 0){ 
      UNSUPPORTED;
    }
  }else{
    DP2("Immediate: RN = %d, RD = %d\n",DPIMM_INFO.Rn, DPIMM_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
    ADD_WORD((uint32_t)DPIMM_INFO.imm);
    
    if(DPIMM_INFO.rotate != 0){
      DP_ASSERT(DPIMM_INFO.S == FALSE,"Rotate with carry not supported\n");
      ADD_BYTE(X86_OP_ROR_RM32);
      ADD_BYTE(0xC8); /* MOD R/M EAX, /1 */
      ADD_BYTE(DPIMM_INFO.rotate * 2);
      DP1("Rotating by %d\n",DPIMM_INFO.rotate * 2);
    }

    ADD_BYTE(X86_OP_AND_MEM32_TO_EAX);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  if(DPREG_INFO.S == TRUE){
    ADD_BYTE(X86_OP_PUSHF);
    ADD_BYTE(X86_OP_POP_MEM32);
    ADD_BYTE(0x05); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    LOG_INSTR(instInfo.pX86Addr,count);
    DP("Storing Flags\n");
  }

  return count;
}

OPCODE_HANDLER_RETURN
eorHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  DP_ASSERT(0,"Eor not supported\n");
  return count;
}

OPCODE_HANDLER_RETURN
subHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(DPREG_INFO.S == TRUE){
    DP("Updating flags\n");

    ADD_BYTE(X86_OP_PUSH_MEM32);
    ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    ADD_BYTE(X86_OP_POPF);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  if(instInfo.immediate == FALSE){
    DP2("Register: RN = %d\nRD = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);

    ADD_BYTE(X86_OP_SUB_MEM32_FROM_EAX);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rm]);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPREG_INFO.shiftAmt != 0){ 
      UNSUPPORTED;
    }
  }else{
    DP2("Immediate: RN = %d, RD = %d\n",DPIMM_INFO.Rn, DPIMM_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
    ADD_WORD((uint32_t)DPIMM_INFO.imm);

    if(DPIMM_INFO.rotate != 0){
      DP_ASSERT(DPIMM_INFO.S == FALSE,"Rotate with carry not supported\n");
      ADD_BYTE(X86_OP_ROR_RM32);
      ADD_BYTE(0xC8); /* MOD R/M EAX, /1 */
      ADD_BYTE(DPIMM_INFO.rotate * 2);
      DP1("Rotating by %d\n",DPIMM_INFO.rotate * 2);
    }

    ADD_BYTE(X86_OP_MOV_TO_REG);
    ADD_BYTE(0xD0); /* MOD R/M EAX to EDX */

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rn]);

    ADD_BYTE(X86_OP_SUB_RM32_FROM_REG);
    ADD_BYTE(0xC2); /* MOD RM EDX from EAX */

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  if(DPREG_INFO.S == TRUE){
    ADD_BYTE(X86_OP_PUSHF);
    ADD_BYTE(X86_OP_POP_MEM32);
    ADD_BYTE(0x05); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  return count;
}

OPCODE_HANDLER_RETURN
rsbHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(DPREG_INFO.S == TRUE){
    DP("Updating flags\n");

    ADD_BYTE(X86_OP_PUSH_MEM32);
    ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    ADD_BYTE(X86_OP_POPF);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  if(instInfo.immediate == FALSE){
    DP2("Register: RN = %d\nRD = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rm]);

    ADD_BYTE(X86_OP_SUB_MEM32_FROM_EAX);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPREG_INFO.shiftAmt != 0){ 
      UNSUPPORTED;
    }
  }else{
    DP2("Immediate: RN = %d, RD = %d\n",DPIMM_INFO.Rn, DPIMM_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
    ADD_WORD((uint32_t)DPIMM_INFO.imm);

    ADD_BYTE(X86_OP_SUB_MEM32_FROM_EAX);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rn]);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPIMM_INFO.rotate != 0){ 
      UNSUPPORTED;
    }
  }

  if(DPREG_INFO.S == TRUE){
    ADD_BYTE(X86_OP_PUSHF);
    ADD_BYTE(X86_OP_POP_MEM32);
    ADD_BYTE(0x05); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  return count;
}

OPCODE_HANDLER_RETURN
addHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  /*
  // FIXME: This check should be done differently depending on whether
  // the instruction is a register instruction or an immediate instruction.
  */
  if(DPREG_INFO.S == TRUE){
    DP("Updating flags\n");

    ADD_BYTE(X86_OP_PUSH_MEM32);
    ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    ADD_BYTE(X86_OP_POPF);
    LOG_INSTR(instInfo.pX86Addr,count);
    DP("Loading Flags\n");
  }

  if(instInfo.immediate == FALSE){
    DP2("Register: RN = %d\nRD = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rm]);

    if(DPREG_INFO.shiftAmt != 0){
      /*
      // The Rm operand needs to be shifted. The amount of shift may either
      // be an immediate value or a value in a register. Further, the shift
      // may be of one of 5 types. Handle all this here.
      */
      if(DPREG_INFO.shiftImm == TRUE){
        if(DPREG_INFO.shiftType == LSL){
          ADD_BYTE(X86_OP_SHL);
          ADD_BYTE(0xE0); /* MOD R/M eax /4 */
          ADD_BYTE(DPREG_INFO.shiftAmt);
        }
      }else{
        UNSUPPORTED;
      }
    }

    ADD_BYTE(X86_OP_ADD_MEM32_TO_EAX);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);
  }else{
    DP2("Immediate: RN = %d, RD = %d\n",DPIMM_INFO.Rn, DPIMM_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
    ADD_WORD((uint32_t)DPIMM_INFO.imm);
    
    if(DPIMM_INFO.rotate != 0){
      DP_ASSERT(DPIMM_INFO.S == FALSE,"Rotate with carry not supported\n");
      ADD_BYTE(X86_OP_ROR_RM32);
      ADD_BYTE(0xC8); /* MOD R/M EAX, /1 */
      ADD_BYTE(DPIMM_INFO.rotate * 2);
      DP1("Rotating by %d\n",DPIMM_INFO.rotate * 2);
    }

    ADD_BYTE(X86_OP_ADD_MEM32_TO_EAX);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  if(DPREG_INFO.S == TRUE){
    ADD_BYTE(X86_OP_PUSHF);
    ADD_BYTE(X86_OP_POP_MEM32);
    ADD_BYTE(0x05); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    LOG_INSTR(instInfo.pX86Addr,count);
    DP("Storing Flags\n");
  }

  return count;
}

OPCODE_HANDLER_RETURN
adcHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  DP_ASSERT(0,"Adc not supported\n");
  return count;
}

OPCODE_HANDLER_RETURN
sbcHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  DP_ASSERT(0,"Sbc not supported\n");
  return count;
}

OPCODE_HANDLER_RETURN
rscHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  DP_ASSERT(0,"Rsc not supported\n");
  return count;
}

OPCODE_HANDLER_RETURN
tstHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  DP_ASSERT(0,"tst not supported\n");
  return count;
}

OPCODE_HANDLER_RETURN
teqHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  return count;
}

OPCODE_HANDLER_RETURN
cmpHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(DPREG_INFO.S == TRUE){
    DP("Updating flags\n");

    ADD_BYTE(X86_OP_PUSH_MEM32);
    ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    ADD_BYTE(X86_OP_POPF);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  if(instInfo.immediate == FALSE){
    DP2("Register: Rn = %d, Rm = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rm);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rm]);

    ADD_BYTE(X86_OP_CMP_MEM32_WITH_REG);
    ADD_BYTE(0x05); /* MODR/M - EAX */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPREG_INFO.shiftAmt != 0){ 
      UNSUPPORTED;
    }
  }else{
    DP2("Immediate: Rn = %d, Rm = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rm);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rn]);

    ADD_BYTE(X86_OP_CMP32_WITH_EAX);
    ADD_WORD((uint32_t)DPIMM_INFO.imm);

    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPIMM_INFO.rotate != 0){ 
      UNSUPPORTED;
    }
  }

  if(DPREG_INFO.S == TRUE){
    ADD_BYTE(X86_OP_PUSHF);
    ADD_BYTE(X86_OP_POP_MEM32);
    ADD_BYTE(0x05); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  return count;
}

OPCODE_HANDLER_RETURN
cmnHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(DPREG_INFO.S == TRUE){
    DP("Updating flags\n");

    ADD_BYTE(X86_OP_PUSH_MEM32);
    ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    ADD_BYTE(X86_OP_POPF);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  if(instInfo.immediate == FALSE){
    DP2("Register: Rn = %d, Rm = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rm);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);

    ADD_BYTE(X86_OP_ADD_MEM32_TO_EAX);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rm]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPREG_INFO.shiftAmt != 0){ 
      UNSUPPORTED;
    }
  }else{
    DP2("Immediate: Rn = %d, Rm = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rm);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rn]);

    ADD_BYTE(X86_OP_CMP32_WITH_EAX);
    ADD_WORD((uint32_t)DPIMM_INFO.imm * -1);
    DP1("Comparing with 0x%x\n",((uint32_t)DPIMM_INFO.imm * -1));

    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPIMM_INFO.rotate != 0){ 
      UNSUPPORTED;
    }
  }

  if(DPREG_INFO.S == TRUE){
    ADD_BYTE(X86_OP_PUSHF);
    ADD_BYTE(X86_OP_POP_MEM32);
    ADD_BYTE(0x05); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  return count;
}

OPCODE_HANDLER_RETURN
orrHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  DP_ASSERT(0,"Orr not supported\n");
  return count;
}

OPCODE_HANDLER_RETURN
movHandler(void *pInst){
  uint8_t count = 0;
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP2("Register: Rm = %d, Rd = %d\n",DPREG_INFO.Rm, DPREG_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rm]);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    /*
    // If the destination is the PC, this may be the end of a basic block.
    // In case this is a conditional instruction and the condition fails,
    // execution should continue at the next instruction. Therefore, the
    // conditional mov can essentially be treated as a branch, where there is
    // a taken case and a not-taken case.
    */
    if(DPREG_INFO.Rd == 15){
      ((struct decodeInfo_t *)pInst)->endBB = TRUE;

      /*
      // FIXME: This can be optimized.
      */
      ADD_BYTE(X86_OP_PUSH_MEM32);
      ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
      ADD_WORD((uintptr_t)&PC);
      ADD_BYTE(X86_OP_POP_MEM32);
      ADD_BYTE(0x05); /* MOD R/M for POP - 0x8F /0 */
      ADD_WORD((uintptr_t)&nextBB);

#ifndef NOCHAINING
      /*
      // Load from register cannot be chained.
      */
      ADD_BYTE(X86_OP_MOV_IMM_TO_MEM32);
      ADD_BYTE(0x05); /* MOD R/M for mov imm32 to rm32 0xC7 /0 */
      ADD_WORD((uintptr_t)&pTakenCalloutSourceLoc);
      ADD_WORD(0x00000000);
#endif /* NOCHAINING */

      ADD_BYTE(X86_OP_CALL);
      ADD_WORD((uintptr_t)(
        (intptr_t)&callEndBBTaken - (intptr_t)(instInfo.pX86Addr + count + 4)
      ));
      LOG_INSTR(instInfo.pX86Addr,count);
    }

    if(DPREG_INFO.shiftAmt != 0){
      UNSUPPORTED;
    }
  }else{
    DP1("Immediate: Rd = %d\n", DPIMM_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
    ADD_WORD((uint32_t)DPIMM_INFO.imm);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPIMM_INFO.rotate != 0){
      UNSUPPORTED;
    }
  }

  return count;
}

OPCODE_HANDLER_RETURN
bicHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(instInfo.immediate == FALSE){
    DP("Register bic\n");
  }else{
    DP("Immediate bic\n");
  }

  DP_ASSERT(0,"Bic not supported\n");
  return count;
}

OPCODE_HANDLER_RETURN
mvnHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  /*
  // FIXME: This check should be done differently depending on whether
  // the instruction is a register instruction or an immediate instruction.
  */
  if(DPREG_INFO.S == TRUE){
    DP("Updating flags\n");

    ADD_BYTE(X86_OP_PUSH_MEM32);
    ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    ADD_BYTE(X86_OP_POPF);
    LOG_INSTR(instInfo.pX86Addr,count);
    DP("Loading Flags\n");
  }

  if(instInfo.immediate == FALSE){
    DP2("Register: RM = %d\nRD = %d\n",DPREG_INFO.Rm, DPREG_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rm]);

    /*
    // FIXME:
    // Complement with impact on FLAGS. Note that in the case of the x86
    // the overflow flag gets cleared. In the ARM, the overflow flag
    // remains unaffected.
    */
    ADD_BYTE(X86_OP_XOR_IMM32_AND_EAX);
    ADD_WORD(0xFFFFFFFF);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPREG_INFO.shiftAmt != 0){ 
      UNSUPPORTED;
    }
  }else{
    DP2("Immediate: Value = %d, RD = %d\n",DPIMM_INFO.imm, DPIMM_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
    ADD_WORD((uint32_t)DPIMM_INFO.imm);
    
    if(DPIMM_INFO.rotate != 0){
      DP_ASSERT(DPIMM_INFO.S == FALSE,"Rotate with carry not supported\n");
      ADD_BYTE(X86_OP_ROR_RM32);
      ADD_BYTE(0xC8); /* MOD R/M EAX, /1 */
      ADD_BYTE(DPIMM_INFO.rotate * 2);
      DP1("Rotating by %d\n",DPIMM_INFO.rotate * 2);
    }

    /*
    // FIXME:
    // Complement with impact on FLAGS. Note that in the case of the x86
    // the overflow flag gets cleared. In the ARM, the overflow flag
    // remains unaffected.
    */
    ADD_BYTE(X86_OP_XOR_IMM32_AND_EAX);
    ADD_WORD(0xFFFFFFFF);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  if(DPREG_INFO.S == TRUE){
    ADD_BYTE(X86_OP_PUSHF);
    ADD_BYTE(X86_OP_POP_MEM32);
    ADD_BYTE(0x05); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    LOG_INSTR(instInfo.pX86Addr,count);
    DP("Storing Flags\n");
  }

  return count;
}

OPCODE_HANDLER_RETURN
swiHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  /* copy system call number to EAX */
  ADD_BYTE(X86_OP_MOV_TO_EAX);
  // ADD_WORD(...);

  /* copy R0 -> EBX */
  ADD_BYTE(X86_OP_MOV_FROM_REG);
  ADD_BYTE(0x1D);
  ADD_WORD((uintptr_t)&regFile[0]);

  /* copy R1 -> ECX */
  ADD_BYTE(X86_OP_MOV_FROM_REG);
  ADD_BYTE(0x0D);
  ADD_WORD((uintptr_t)&regFile[1]);

  /* copy R2 -> EDX */
  ADD_BYTE(X86_OP_MOV_FROM_REG);
  ADD_BYTE(0x15);
  ADD_WORD((uintptr_t)&regFile[2]);

  /* copy R3 -> ESI */
  ADD_BYTE(X86_OP_MOV_FROM_REG);
  ADD_BYTE(0x35);
  ADD_WORD((uintptr_t)&regFile[3]);

  /* copy R4 -> EDI */
  ADD_BYTE(X86_OP_MOV_FROM_REG);
  ADD_BYTE(0x3D);
  ADD_WORD((uintptr_t)&regFile[4]);

  /* invoke X86 system call */
  ADD_BYTE(X86_OP_INT_VECTOR);
  ADD_BYTE(X86_OP_SYSTEM_CALL);

  /* get result from EAX and store it in R0 */
  ADD_BYTE(X86_OP_MOV_FROM_EAX);
  ADD_WORD((uintptr_t)&regFile[0]);
  LOG_INSTR(instInfo.pX86Addr,count);

  DP_ASSERT(0, "Swi not supported\n");
  return count;
}


/*****************************************************************************
This is a list of typical ARM instructions and their planned mapping to X86
instructions.
------------------------------------------------------------------------------
ARM			X86			Notes
------------------------------------------------------------------------------
mov ip, sp		mov %eax,<addr1>	Move the contents of IP to SP.
			mov <addr2>,%eax	Both of these are global vars
						as far as the x86 instruction
						instruction is concerned.	
stmdb sp!,{...}
ldmia sp,{...}
ldr
str
sub                     mov %eax,<addr1>        Move the contents of the 
                        sub %eax, $const        global var representing reg
                        mov <addr2>,%eax        into eax. Do the subtraction
                                                and then store the result. 
mov
add
bl
*****************************************************************************/
