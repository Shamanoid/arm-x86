#include <stdint.h>
#include "ArmX86Debug.h"
#include "ArmX86Decode.h"
#include "ArmX86Types.h"

#define COND_MASK               0xF0000000  // Mask for the condition field
#define COND_AL			0xE0000000  // Condition - Always

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
//
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
#define OPCODE_SHIFT            20

#define RD_SHIFT                11
#define RN_SHIFT                15

#define RD(x)                   (((x) & 0x000F0000) >> RD_SHIFT)
#define RN(x)                   (((x) & 0x0000F000) >> RN_SHIFT)
#define RM(x)                   ((x) & 0x0000000F)

typedef void (*opcodeHandler_t)(void *);
opcodeHandler_t opcodeHandler[NUM_OPCODES] = {
  andHandler, eorHandler, subHandler, rsbHandler,
  addHandler, adcHandler, sbcHandler, rscHandler,
  tstHandler, teqHandler, cmpHandler, cmnHandler,
  orrHandler, movHandler, bicHandler, mvnHandler
};

void armX86Decode(uint32_t *instr){
  bool conditional = FALSE;

  DP_HI;

  //
  // First check the condition field. Set a flag in case the instruction
  // is to be executed conditionally.
  //
  if((*instr & COND_MASK) != COND_AL){
    conditional = TRUE;
  }

  DP_ASSERT(conditional == FALSE,"Encountered conditional instruction\n");

  //
  // There are usually two source operands and one destination register for
  // data processing instruction. The compare, test and move instructions
  // are exceptions. Some of them have only one source operand and other
  // do not have a destination register.
  //

  //
  // Instructions are decoded by family
  //
  switch(*instr & INST_TYPE_MASK){
    case INST_TYPE_DP_MISC:
      //
      // There are two conditions that help distinguish an instruction
      // as a data processing instruction:
      //   Bit 4 and Bit 7 should not both be '1'
      //   In case the top two bits of the opcode are "10", the S bit
      //   should be a '1'
      //
      if(((*instr & 0x01900000) != 0x01000000) && 
         ((*instr & 0x00000090) != 0x00000090)){
           (opcodeHandler[((*instr & OPCODE_MASK) >> OPCODE_SHIFT)])((void *)instr);
      }
    break;
    case INST_TYPE_IMM_UNDEF:
    break;
    case INST_TYPE_LSIMM:
    break;
    case INST_TYPE_LSR_UNDEF:
    break;
    case INST_TYPE_LSMULT:
    break;
    case INST_TYPE_BRCH:
    break;
    case INST_TYPE_COPLS:
    break;
    case INST_TYPE_COP_SWI:
    break;
    default:
    break;
  }

  DP_BYE;
}

void andHandler(void *inst){
  DP("\tand\n");
}

void eorHandler(void *inst){
  DP("\teor\n");
}

void subHandler(void *inst){
  DP("\tsub\n");
}

void rsbHandler(void *inst){
  DP("\trsb\n");
}

void addHandler(void *inst){
  DP("\tadd\n");
}

void adcHandler(void *inst){
  DP("\tadc\n");
}

void sbcHandler(void *inst){
  DP("\tsbc\n");
}

void rscHandler(void *inst){
  DP("\trsc\n");
}

void tstHandler(void *inst){
  DP("\ttst\n");
}

void teqHandler(void *inst){
  DP("\tteq\n");
}

void cmpHandler(void *inst){
  DP("\tcmp\n");
}

void cmnHandler(void *inst){
  DP("\tcmn\n");
}

void orrHandler(void *inst){
  DP("\torr\n");
}

void movHandler(void *inst){
  DP("\tmov\n");
}

void bicHandler(void *inst){
  DP("\tbic\n");
}

void mvnHandler(void *inst){
  DP("\tmvn\n");
}

