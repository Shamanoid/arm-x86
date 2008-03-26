#include <stdint.h>
#include "ArmX86Debug.h"
#include "ArmX86Decode.h"
#include "ArmX86Types.h"

#define COND_MASK               0xF0000000  /* Mask for the condition field */
#define COND_AL			0xE0000000  /* Condition - Always */
#define COND_SHIFT              28

/*
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
  0x80000, 0x80000, 0x80000, 0x80000,
  0x80000, 0x80000, 0x80000, 0x80000,
  0x80000, 0x80000, 0x80000, 0x80000,
  0x80000, 0x80000, 0x80000, 0x80000
};

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

    armInst = *pArmPC;
    /*
    // First check the condition field. Set a flag in case the instruction
    // is to be executed conditionally.
    */
    if((armInst & COND_MASK) != COND_AL){
      bConditional = TRUE;
    }else{
      bConditional = FALSE;
    }

    DP_ASSERT(bConditional == FALSE,"Encountered conditional instruction\n");

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
          DPREG_INFO.cond = ((armInst & COND_MASK) >> COND_SHIFT);
          DPREG_INFO.Rn = RN(armInst);
          DPREG_INFO.Rm = RM(armInst);
          DPREG_INFO.Rd = RD(armInst);
          DPREG_INFO.S = ((armInst & BIT20_MASK) > 0?TRUE:FALSE);
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
          DPIMM_INFO.cond = ((armInst & COND_MASK) >> COND_SHIFT);
          DPIMM_INFO.Rn = RN(armInst);
          DPIMM_INFO.Rd = RD(armInst);
          DPIMM_INFO.rotate = ROTATE(armInst);
          DPIMM_INFO.imm = (armInst & 0x000000FF);
          DPIMM_INFO.S = ((armInst & BIT20_MASK) >0?TRUE:FALSE);
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
        LSIMM_INFO.cond = ((armInst & COND_MASK) >> COND_SHIFT);
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
          LSREG_INFO.cond = ((armInst & COND_MASK) >> COND_SHIFT);
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
        LSMULT_INFO.cond = ((armInst & COND_MASK) >> COND_SHIFT);
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
        BRCH_INFO.cond = ((armInst & COND_MASK) >> COND_SHIFT);
        BRCH_INFO.L = ((armInst & BIT24_MASK) > 0?TRUE:FALSE);
        BRCH_INFO.offset = (armInst & OFFSET_MASK);
        instInfo.pX86Addr = pX86PC;
        x86InstCount = brchHandler((void *)&instInfo);
        pX86PC += x86InstCount;
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

  DP_BYE;
}


#define X86_OP_MOV_TO_EAX       0xA1
#define X86_OP_MOV_FROM_EAX     0xA3
#define X86_OP_SUB32_FROM_EAX   0x2D
#define X86_OP_MEM32_FROM_EAX   0x2B
#define X86_OP_MOV_TO_REG       0x8B
#define X86_OP_MOV_FROM_REG     0x89
#define X86_OP_MOV_IMM_TO_EAX   0xB8

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
  struct decodeInfo_t instInfo __attribute__((unused))
    = *(struct decodeInfo_t *)pInst;
  uint8_t count = 0;

  DP("Branch\n");

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

  if(instInfo.immediate == FALSE){
    DP("Register ");
    printf("\tRN = %d\nRD = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_TO_EAX);
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);

    ADD_BYTE(X86_OP_MEM32_FROM_EAX);
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

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }

  return 0;
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
