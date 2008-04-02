#ifndef _ARMX86_DECODEPRIVATE_H
#define _ARMX86_DECODEPRIVATE_H

#include <stdint.h>
#include <assert.h>
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
#define COND_AL                 0xE0000000  /* Condition - Always */
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

/*
// Set of macros defining x86 opcodes.
*/
#define X86_OP_MOV_TO_EAX            0xA1
#define X86_OP_MOV_FROM_EAX          0xA3
#define X86_OP_SUB32_FROM_EAX        0x2D
#define X86_OP_ADD32_TO_EAX          0x05
#define X86_OP_SUB_MEM32_FROM_EAX    0x2B
#define X86_OP_ADD_MEM32_TO_EAX      0x03
#define X86_OP_MOV_TO_REG            0x8B
#define X86_OP_MOV_FROM_REG          0x89
#define X86_OP_MOV_IMM_TO_EAX        0xB8
#define X86_OP_PUSH_MEM32            0xFF
#define X86_OP_POPF                  0x9D
#define X86_OP_POP_MEM32             0x8F
#define X86_OP_PUSHF                 0x9C
#define X86_OP_PUSH_IMM32            0x68
#define X86_OP_CMP_MEM32_WITH_REG    0x39
#define X86_OP_CMP32_WITH_EAX        0x3D
#define X86_OP_CALL                  0xE8
#define X86_PRE_JCC                  0x0F
#define X86_OP_JNE                   0x85
#define X86_OP_JGE                   0x8D
#define X86_OP_JLE                   0x8E
#define X86_OP_JG                    0x8F
#define X86_OP_JL                    0x8C

#define UNSUPPORTED              DP_ASSERT(0,"Unsupported ARM instruction\n")
typedef enum {
  LSL,
  LSR,
  ASR,
  ROR,
  RRX
}shift_t;

struct decodeInfo_t{
  union armInstInfo_u{
    struct {
      bool P;
      bool U;
      bool S;
      bool W;
      bool L;
      uint8_t Rn;
      uint16_t regList;
    }lsmult;

    struct {
      bool S;
      uint8_t Rn;
      uint8_t Rd;
      uint8_t Rm;
      shift_t shiftType;
      uint8_t shiftAmt;
      uint8_t Rs;
      bool shiftImm; /* True => Imm Shift, False => Reg shift */
    }dpreg;

    struct {
      bool S;
      uint8_t Rn;
      uint8_t Rd;
      uint8_t rotate;
      uint8_t imm;
    }dpimm;

    struct {
      bool P;
      bool U;
      bool B;
      bool W;
      bool L;
      uint8_t Rn;
      uint8_t Rd;
      uint8_t Rm;
      shift_t shiftType;
      uint8_t shiftAmt;
    }lsreg;

    struct {
      bool P;
      bool U;
      bool B;
      bool W;
      bool L;
      uint8_t Rn;
      uint8_t Rd;
      uint16_t imm;
    }lsimm;

    struct {
      bool L;
      int32_t offset;
    }branch;
  }armInstInfo;
  uint8_t cond;
  bool immediate; /* True => DPIMM */
  bool endBB; /* Instruction signals end of basic block */
  uint8_t* pX86Addr;
  uint32_t* pArmAddr;
};


#define OPCODE_HANDLER_RETURN   int
void decodeBasicBlock();

OPCODE_HANDLER_RETURN andHandler(void *pInst);
OPCODE_HANDLER_RETURN eorHandler(void *pInst);
OPCODE_HANDLER_RETURN subHandler(void *pInst);
OPCODE_HANDLER_RETURN rsbHandler(void *pInst);
OPCODE_HANDLER_RETURN addHandler(void *pInst);
OPCODE_HANDLER_RETURN adcHandler(void *pInst);
OPCODE_HANDLER_RETURN sbcHandler(void *pInst);
OPCODE_HANDLER_RETURN rscHandler(void *pInst);
OPCODE_HANDLER_RETURN tstHandler(void *pInst);
OPCODE_HANDLER_RETURN teqHandler(void *pInst);
OPCODE_HANDLER_RETURN cmpHandler(void *pInst);
OPCODE_HANDLER_RETURN cmnHandler(void *pInst);
OPCODE_HANDLER_RETURN orrHandler(void *pInst);
OPCODE_HANDLER_RETURN movHandler(void *pInst);
OPCODE_HANDLER_RETURN bicHandler(void *pInst);
OPCODE_HANDLER_RETURN mvnHandler(void *pInst);
int lsmHandler(void *pInst);
int lsimmHandler(void *pInst);
int lsregHandler(void *pInst);
int brchHandler(void *pInst);

#endif /* _ARMX86_DECODEPRIVATE_H */
