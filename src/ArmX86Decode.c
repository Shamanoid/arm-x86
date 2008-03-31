#include <stdint.h>
#include "ArmX86Debug.h"
#include "ArmX86Decode.h"
#include "ArmX86Types.h"

/*
// Set of macros and global variables to deal with instructions and opcodes
//
// For purposes of instruction decoding, the instructions are divided into
// families as described here. This classification is based on the bits
// 27 down to 25 in each instruction.
//  1. "000" DP_MISC
//    Data Processing shift instructions
//    Miscellaneous instructions
//  2. "001" IMM_UNDEF
//    Move and DP Immediate instructions
//    Undefined instructions
//  3. "010" LSIMM
//    Load/Store with immediate offset
//  4. "011" LSR_UNDEF
//    Load/Store register instructions
//    More Undefined instructions
//  5. "100" LSMULT
//    Load/Store Multiple
//  6. "101" BRCH
//    Branch instructions
//  7. "110" COPLS
//    Coprocessor Load/Store instructions
//  8. "111" COP_SWI
//    Coprocessor instructions
//    Software Interrupt
*/
#define COND_MASK               0xF0000000  /* Mask for the condition field */
#define COND_AL			0xE0000000  /* Condition - Always */
#define COND_SHIFT              28

#define INST_TYPE_MASK          0x0E000000  // What type of instruction?
#define INST_TYPE_DP_MISC       0x00000000
#define INST_TYPE_IMM_UNDEF     0x02000000
#define INST_TYPE_LSIMM         0x04000000
#define INST_TYPE_LSR_UNDEF     0x06000000
#define INST_TYPE_LSMULT        0x08000000
#define INST_TYPE_BRCH          0x0A000000
#define INST_TYPE_COPLS         0x0C000000
#define INST_TYPE_COP_SWI       0x0E000000

#define NUM_OPCODES             16
#define OPCODE_MASK             0x01E00000  // Mask for the opcode field
#define OPCODE_AND              0x00000000  // AND
#define OPCODE_EOR              0x00200000  // EOR
#define OPCODE_SUB              0x00400000  // SUB
#define OPCODE_RSB              0x00600000  // Reverse Subtract
#define OPCODE_ADD              0x00800000  // ADD
#define OPCODE_ADC              0x00A00000  // Add with carry
#define OPCODE_SBC              0x00C00000  // Subtract with carry
#define OPCODE_RSC              0x00E00000  // Reverse Subtract with carry
#define OPCODE_TST              0x01000000  // Test
#define OPCODE_TEQ              0x01200000  // Test equivalence
#define OPCODE_CMP              0x01400000  // Compare
#define OPCODE_CMN              0x01600000  // Compare Negated
#define OPCODE_ORR              0x01800000  // Logical (inclusive OR)
#define OPCODE_MOV              0x01A00000  // Move
#define OPCODE_BIC              0x01C00000  // Bit clear
#define OPCODE_MVN              0x01E00000  // Move not
#define OPCODE_SHIFT            21

/*
// Set of macros and global variables to deal with registers
*/
#define RD_SHIFT                12
#define RN_SHIFT                16
#define RS_SHIFT                8

#define RD(x)                   (((x) & 0x0000F000) >> RD_SHIFT)
#define RN(x)                   (((x) & 0x000F0000) >> RN_SHIFT)
#define RS(x)                   (((x) & 0x00000F00) >> RS_SHIFT)
#define RM(x)                   ((x) & 0x0000000F)
#define ROTATE(x)               RS(x)
#define SHIFT_AMT_MASK          0x00000F80
#define SHIFT_AMT_SHIFT         7
#define SHIFT_TYPE_MASK         0x00000060
#define SHIFT_TYPE_SHIFT        5
#define OFFSET_MASK             0x00FFFFFF
#define BIT24_MASK              0x01000000
#define BIT23_MASK              0x00800000 
#define BIT22_MASK              0x00400000
#define BIT21_MASK              0x00200000
#define BIT20_MASK              0x00100000

#define NUM_ARM_REGISTERS       16  
#define R13                     regFile[13]
#define R14                     regFile[14]
#define R15                     regFile[15]

#define SP                      R13
#define LR                      R14
#define IP                      R15

uint32_t regFile[NUM_ARM_REGISTERS] = {
  0x0, 0x1, 0x2, 0x3,
  0x4, 0x5, 0x6, 0x7,
  0x8, 0x9, 0xA, 0xB,
  0xC, 0xD, 0xE, 0xF
};

/*
// Set of macros and global variables to deal with conditions
*/
uint32_t cpsr;     /* ARM Program Status Register for user mode */
uint32_t x86Flags; /* x86 Flag Register */

#define N_FLAG_MASK             0x80000000
#define Z_FLAG_MASK             0x40000000
#define C_FLAG_MASK             0x20000000
#define V_FLAG_MASK             0x10000000

#define NEGATIVE(x)             (((x) & N_FLAG_MASK) > 0?1:0)
#define ZERO(x)                 (((x) & Z_FLAG_MASK) > 0?1:0)
#define CARRY(x)                (((x) & C_FLAG_MASK) > 0?1:0)
#define OVERFLOW(x)             (((x) & V_FLAG_MASK) > 0?1:0)

#define EQ                      ZER0(cpsr)      /* Z = 1            */
#define NE                      !EQ             /* Z = 0            */
#define CS                      CARRY(cpsr)     /* C = 1            */
#define CC                      !CS             /* C = 0            */
#define MI                      NEGATIVE(cpsr)  /* N = 1            */
#define PL                      !MI             /* N = 0            */
#define VS                      OVERFLOW(cpsr)  /* V = 1            */
#define VC                      !VS             /* V = 0            */
#define HI                      (CS && NE)      /* C = 1 && Z = 0   */
#define LS                      (CC || EQ)      /* C = 0 || Z = 1   */
#define GE                      !(MI ^ VS)      /* N == V           */
#define LT                      (MI ^ VS)       /* N != V           */
#define GT                      (NE && GE)      /* Z = 0 and N == V */
#define LE                      (EQ || LT)      /* Z = 1 or  N != V */
#define AL                      0xE

#define COND_EQ                 0
#define COND_NE                 1
#define COND_CS                 2
#define COND_CC                 3
#define COND_MI                 4 
#define COND_PL                 5
#define COND_VS                 6
#define COND_VC                 7
#define COND_HI                 8
#define COND_LS                 9
#define COND_GE                 10
#define COND_LT                 11
#define COND_GT                 12 
#define COND_LE                 13
#define COND_UNDEF              15

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

/*
// Set of macros defining x86 opcodes.
*/
#define X86_OP_MOV_TO_EAX            0xA1
#define X86_OP_MOV_FROM_EAX          0xA3
#define X86_OP_SUB32_FROM_EAX        0x2D
#define X86_OP_SUB_MEM32_FROM_EAX    0x2B
#define X86_OP_MOV_TO_REG            0x8B
#define X86_OP_MOV_FROM_REG          0x89
#define X86_OP_MOV_IMM_TO_EAX        0xB8
#define X86_OP_PUSH_MEM32            0xFF
#define X86_OP_POPF                  0x9D
#define X86_OP_POP_MEM32             0x8F
#define X86_OP_PUSHF                 0x9C
#define X86_OP_CMP_MEM32_WITH_REG    0x29
#define X86_OP_CMP32_WITH_EAX        0x3D
#define X86_OP_CALL                  0xE8
#define X86_PRE_JCC                  0x0F
#define X86_OP_JNE                   0x85
#define X86_OP_JG                    0x8F
#define X86_OP_JL                    0x8C

#ifdef DEBUG

#define LOG_INSTR(addr,count) { \
  int i;                        \
  printf("%p: ",addr);          \
  for(i=0; i<(count); i++){     \
    printf(" %x",*((addr)+i));  \
  }                             \
  printf("\n");                 \
}                               \

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
  printf("=========\n");                         \
}                                                \

#else

#define LOG_INSTR(addr,count) 
#define DISPLAY_REGS 

#endif /* DEBUG */

void callEndBBTaken(){
  DP_HI;
  DP_BYE;
}

void callEndBBNotTaken(){
  DP_HI;
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

  printf("cond = %d\n",instInfo.cond);
  switch(instInfo.cond){
    case COND_EQ:
      /* jne */
      ADD_BYTE(X86_PRE_JCC)
      ADD_BYTE(X86_OP_JNE)
    break;
    case COND_NE:
    break;
    case COND_CS:
    break;
    case COND_CC:
    break;
    case COND_MI:
    break;
    case COND_PL:
    break;
    case COND_VS:
    break;
    case COND_VC:
    break;
    case COND_HI:
    break;
    case COND_LS:
    break;
    case COND_GE:
      /* jl */
      ADD_BYTE(X86_PRE_JCC)
      ADD_BYTE(X86_OP_JL)
    break;
    case COND_LT:
    break;
    case COND_GT:
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
// Include a couple of profiling variable. These are not really
// indicative because a static count of instructions that modify
// flags is not as important as the run-count for instructions
// that do. But keep them around anyway.
*/
uint32_t sEq1Count = 0;
uint32_t sEq0Count = 0;

#endif /* DEBUG */

void armX86Decode(const struct map_t *memMap){
  bool bConditional = FALSE;
  struct decodeInfo_t instInfo;
  uint32_t armInst;

  /*
  // x86InstCount is number of bytes of x86 instructions
  */
  uint32_t x86InstCount;
  uint32_t *pArmPC = memMap->pArmInstr;
  uint8_t *pX86PC = memMap->pX86Instr;
  translator x86Translator = (translator)memMap->pX86Instr;
  uint8_t *pCondJumpOffsetAddr = 0;
  uint8_t count = 0;

  DP_HI;

  DP1("x86PC = %p\n",pX86PC);

  /*
  // FIXME: This is a temporary initialization of the stack pointer
  // to some place that the os is happy letting me access.
  */
  regFile[0] = (uintptr_t)memMap->pArmStackPtr;
  SP = (uintptr_t)memMap->pArmStackPtr;

  instInfo.endBB = FALSE;

  while(instInfo.endBB == FALSE){
    DP1("Processing instruction: 0x%x\n",*pArmPC);

    count = 0;
    armInst = *pArmPC;
    /*
    // First check the condition field. Set a jump in the code if the
    //  instruction is to be executed conditionally.
    */
#if 0
    if((armInst & COND_MASK) != COND_AL){
      bConditional = TRUE;
    }else{
      bConditional = FALSE;
    }
#endif

    instInfo.cond = ((armInst & COND_MASK) >> COND_SHIFT);
    instInfo.pX86Addr = pX86PC;
    if(instInfo.cond != AL){
      x86InstCount = handleConditional((void *)&instInfo);
      pCondJumpOffsetAddr = instInfo.pX86Addr + x86InstCount;
      x86InstCount += 4; /* Reserve space for a 4-byte offset */
      *(uint32_t *)pCondJumpOffsetAddr = 0; /* Set the offset to 0 at first */
      pX86PC += x86InstCount; 
    }

#if 0
    DP_ASSERT(bConditional == FALSE,"Encountered conditional instruction\n");
#endif
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
        // jumping beyond the instruction should equate to a branch that is not
        // taken. So at the instruction 'beyond', I place a call to the handler
        // for the NotTakenBranch. The offset for the conditional jump points
        // to this call instrution.
        */
        ADD_BYTE(X86_OP_CALL);
        ADD_WORD((uintptr_t)(
          (intptr_t)&callEndBBNotTaken - (intptr_t)(pX86PC + 5)
        ));
        pX86PC += 5;

      break;
      case INST_TYPE_COPLS:
        UNSUPPORTED;
      break;
      case INST_TYPE_COP_SWI:
        UNSUPPORTED;
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
  
  /*
  // For the moment, I insert a return following the last instruction
  // that is in the translation cache. Then I make a call to the
  // start of the translation cache.
  */
  DP1("x86PC = %p\n",pX86PC);
  *pX86PC = 0xC3;

  DISPLAY_REGS;

  x86Translator();

  DISPLAY_REGS;

#ifdef DEBUG
  float sEq1Percent = ((100.0 * sEq1Count)/(sEq1Count + sEq0Count));
  float sEq0Percent = ((100.0 * sEq0Count)/(sEq1Count + sEq0Count));

  printf("%.2f percent instructions modify flags\n",sEq1Percent);
  printf("%.2f percent instructions don't modify flags\n",sEq0Percent);
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
  uint8_t i;
  uint32_t disp = 0;

  DP("Load-Store Multiple\n");

  ADD_BYTE(X86_OP_MOV_TO_REG);
  ADD_BYTE(0x15) /* MODR/M - Mov from disp32 to edx */
  ADD_WORD((uintptr_t)&regFile[LSMULT_INFO.Rn]);

  for(i=0; i<NUM_ARM_REGISTERS; i++){
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

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[LSMULT_INFO.Rn]);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  return count;
}

int lsimmHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t *)pInst;
  uint8_t count = 0;

  DP("\tLoad-Store Immediate\n");

  DP_ASSERT(LSIMM_INFO.B != 1, "Byte transfer not supported\n");
  DP_ASSERT(LSIMM_INFO.P != 0, "Post-Indexed addressing not supported\n");
  DP_ASSERT(LSIMM_INFO.W != 1, "Pre-Indexed addressing not supported\n");

  if(LSIMM_INFO.L == 1){
    DP("Load ");
    printf("Rd = %d, Rn = %d\n",LSIMM_INFO.Rd, LSIMM_INFO.Rn);

    ADD_BYTE(X86_OP_MOV_TO_REG);
    ADD_BYTE(0x15) /* MODR/M - Mov from disp32 to edx */
    ADD_WORD((uintptr_t)&regFile[LSIMM_INFO.Rn]);

    ADD_BYTE(X86_OP_MOV_TO_REG);
    ADD_BYTE(0x82) /* MODR/M - Mov from edx + disp32 to eax */
    ADD_WORD((int32_t)LSIMM_INFO.imm * (LSIMM_INFO.U == 0?-1:1));

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[LSIMM_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    /*
    // If the destination is the PC, this is the end of a basic block
    */
    if(LSIMM_INFO.Rd == 15){
      ((struct decodeInfo_t *)pInst)->endBB = TRUE;
    };
  }else{
    DP("Store ");
    printf("Rd = %d, Rn = %d\n",LSIMM_INFO.Rd, LSIMM_INFO.Rn);

    ADD_BYTE(X86_OP_MOV_TO_REG);
    ADD_BYTE(0x15) /* MODR/M - Mov from disp32 to edx */
    ADD_WORD((uintptr_t)&regFile[LSIMM_INFO.Rn]);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[LSIMM_INFO.Rd]);

    ADD_BYTE(X86_OP_MOV_FROM_REG);
    ADD_BYTE(0x82) /* MODR/M - Mov from eax to  edx + disp32 */
    ADD_WORD((int32_t)LSIMM_INFO.imm * (LSIMM_INFO.U == 0?-1:1));
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  return count;
}

int lsregHandler(void *pInst){
  struct decodeInfo_t instInfo __attribute__((unused))
    = *(struct decodeInfo_t *)pInst;
  uint8_t count = 0;

  DP("\tLoad-Store Register-Shift\n");

  return count;
}

int brchHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t *)pInst;
  uint8_t count = 0;

  DP("Branch\n");

  ADD_BYTE(X86_OP_CALL);
  ADD_WORD((uintptr_t)(
    (intptr_t)&callEndBBTaken - (intptr_t)(instInfo.pX86Addr + 5)
  ));

  ((struct decodeInfo_t *)pInst)->endBB = TRUE;

  return count;
}

OPCODE_HANDLER_RETURN
andHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  return 0;
}

OPCODE_HANDLER_RETURN
eorHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  return 0;
}

OPCODE_HANDLER_RETURN
subHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(DPREG_INFO.S == TRUE){
    ADD_BYTE(X86_OP_PUSH_MEM32);
    ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    ADD_BYTE(X86_OP_POPF);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  if(instInfo.immediate == FALSE){
    DP("Register ");
    printf("\tRN = %d\nRD = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rd);

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
    DP("Immediate ");
    printf("RN = %d, RD = %d\n",DPIMM_INFO.Rn, DPIMM_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rn]);

    ADD_BYTE(X86_OP_SUB32_FROM_EAX);
    ADD_WORD((uint32_t)DPIMM_INFO.imm);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPIMM_INFO.rotate != 0){ 
      UNSUPPORTED;
    }
  }
  if(DPREG_INFO.S == 0){
    DP(" WARNING -> Updating FLAGS when prohibited\n");
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

  if(instInfo.immediate == FALSE){
    DP("Register\n");
  }else{
    DP("Immediate\n");
  }

  return 0;
}

OPCODE_HANDLER_RETURN
addHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("Register\n");
  }else{
    DP("Immediate\n");
  }

  return 0;
}

OPCODE_HANDLER_RETURN
adcHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  return 0;
}

OPCODE_HANDLER_RETURN
sbcHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  return 0;
}

OPCODE_HANDLER_RETURN
rscHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  return 0;
}

OPCODE_HANDLER_RETURN
tstHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  return 0;
}

OPCODE_HANDLER_RETURN
teqHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  return 0;
}

OPCODE_HANDLER_RETURN
cmpHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  if(DPREG_INFO.S == TRUE){
    ADD_BYTE(X86_OP_PUSH_MEM32);
    ADD_BYTE(0x35); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    ADD_BYTE(X86_OP_POPF);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  if(instInfo.immediate == FALSE){
    DP("Register ");
    printf("Rn = %d, Rm = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rm);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);

    ADD_BYTE(X86_OP_CMP_MEM32_WITH_REG);
    ADD_BYTE(0x05); /* MODR/M - EAX */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rm]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPREG_INFO.shiftAmt != 0){ 
      UNSUPPORTED;
    }
  }else{
    DP("Immediate ");
    printf("Rn = %d, Rm = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rm);

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

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }
  return 0;
}

OPCODE_HANDLER_RETURN
orrHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  return 0;
}

OPCODE_HANDLER_RETURN
movHandler(void *pInst){
  uint8_t count = 0;
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("Register ");

    printf("Rn = %d, Rd = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPREG_INFO.shiftAmt != 0){
      UNSUPPORTED;
    }
  }else{
    DP("Immediate\n");

    printf("Rd = %d\n", DPIMM_INFO.Rd);

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

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }
  printf("bic\n");

  return 0;
}

OPCODE_HANDLER_RETURN
mvnHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }
  printf("mvn\n");

  return 0;
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
