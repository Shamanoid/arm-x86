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

#define RD(x)                   (((x) & 0x000F0000) >> RD_SHIFT)
#define RN(x)                   (((x) & 0x0000F000) >> RN_SHIFT)
#define RS(x)                   (((x) & 0x00000F00) >> RS_SHIFT)
#define RM(x)                   ((x) & 0x0000000F)
#define ROTATE(x)               RS(x)
#define SHIFT_AMT_MASK          0x00000F80
#define SHIFT_AMT_SHIFT         7
#define SHIFT_TYPE_MASK         0x00000060
#define SHIFT_TYPE_SHIFT        5

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
  0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7,
  0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF
};

typedef OPCODE_HANDLER_RETURN (*opcodeHandler_t)(void *inst);
opcodeHandler_t opcodeHandler[NUM_OPCODES] = {
  andHandler, eorHandler, subHandler, rsbHandler,
  addHandler, adcHandler, sbcHandler, rscHandler,
  tstHandler, teqHandler, cmpHandler, cmnHandler,
  orrHandler, movHandler, bicHandler, mvnHandler
};

#define NUM_ARM_INSTRUCTIONS    20
#define DPREG_INFO              instInfo.armInstInfo.dpreg
#define DPIMM_INFO              instInfo.armInstInfo.dpimm
#define LSMULT_INFO             instInfo.armInstInfo.lsmult
#define LSREG_INFO              instInfo.armInstInfo.lsreg
#define LSIMM_INFO              instInfo.armInstInfo.lsimm

void armX86Decode(uint32_t *pArmInstr, uint8_t *pX86Instr){
  bool bConditional = FALSE;
  struct decodeInfo_t instInfo;
  uint32_t armInst;

  /*
  // armInstCount is number of ARM instructions
  // x86InstCount is number of bytes of x86 instructions
  */
  uint32_t x86InstCount,armInstCount;
  uint32_t *pArmPC = pArmInstr;
  uint8_t *pX86PC = pX86Instr;

  DP_HI;

  for(armInstCount=0; armInstCount < NUM_ARM_INSTRUCTIONS; armInstCount++){
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
  DP_BYE;
}

int lsmHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t *)pInst;
  uint8_t count;

  DP("\tLoad-Store Multiple\n");

  return count;
}

int lsimmHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t *)pInst;
  uint8_t count;

  DP("\tLoad-Store Immediate\n");

  return count;
}

int lsregHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t *)pInst;
  uint8_t count;

  DP("\tLoad-Store Register-Shift\n");

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
  printf("and\n");
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
  printf("eor\n");

  return 0;
}

OPCODE_HANDLER_RETURN
subHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }
  printf("sub\n");

  return 0;
}

OPCODE_HANDLER_RETURN
rsbHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }
  printf("rsb\n");

  return 0;
}

OPCODE_HANDLER_RETURN
addHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }
  printf("add\n");

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
  printf("adc\n");

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
  printf("sbc\n");

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
  printf("rsc\n");

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
  printf("tst\n");

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
  printf("teq\n");

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
  printf("cmp\n");

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
  printf("cmn\n");

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
  printf("orr\n");

  return 0;
}

#define X86_OP_MOV_TO_EAX      0xA1
#define X86_OP_MOV_FROM_EAX    0xA3

OPCODE_HANDLER_RETURN
movHandler(void *pInst){
  uint8_t count = 0;
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;

  if(instInfo.immediate == FALSE){
    DP("\tRegister ");
  }else{
    DP("\tImmediate ");
  }
  printf("mov\n");

  *((uint8_t *)instInfo.pX86Addr + count) = X86_OP_MOV_TO_EAX;
  count++;
  *(uint32_t *)(instInfo.pX86Addr + count) = (uintptr_t)&regFile[DPREG_INFO.Rd];
  count+=4;
  *((uint8_t *)instInfo.pX86Addr + count) = X86_OP_MOV_FROM_EAX;
  count++;
  *(uint32_t *)(instInfo.pX86Addr + count) = (uintptr_t)&regFile[DPREG_INFO.Rn];
  count+=4;

  count = IP + SP;

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
sub
mov
add
bl
*****************************************************************************/
