#include "ArmX86Debug.h"
#include "ArmX86DecodePrivate.h"
#include "codegen.h"

opcodeHandler_t opcodeHandler[NUM_OPCODES] = {
    andHandler, eorHandler, subHandler, rsbHandler,
    addHandler, adcHandler, sbcHandler, rscHandler,
    tstHandler, teqHandler, cmpHandler, cmnHandler,
    orrHandler, movHandler, bicHandler, mvnHandler
};

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

  DP_HI;

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

    if(DPREG_INFO.shiftAmt != 0){ 
      UNSUPPORTED;
    }
  }else{
    DP2("Immediate: RN = %d, RD = %d\n",DPIMM_INFO.Rn, DPIMM_INFO.Rd);

    ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
    ADD_WORD((uint32_t)DPIMM_INFO.imm);
    
    if(DPIMM_INFO.rotate != 0){
      ADD_BYTE(X86_OP_ROR_RM32);
      ADD_BYTE(0xC8); /* MOD R/M EAX, /1 */
      ADD_BYTE(DPIMM_INFO.rotate * 2);
      DP1("Rotating by %d\n",DPIMM_INFO.rotate * 2);
    }

    ADD_BYTE(X86_OP_AND_MEM32_TO_EAX);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);
  }

  if(DPREG_INFO.S == TRUE){
    ADD_BYTE(X86_OP_PUSHF);
    ADD_BYTE(X86_OP_POP_MEM32);
    ADD_BYTE(0x05); /* MOD R/M for PUSH - 0xFF /6 */
    ADD_WORD((uintptr_t)&x86Flags);
    LOG_INSTR(instInfo.pX86Addr,count);
    DP("Storing Flags\n");
  }

  DP_BYE;
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
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rm]);

    ADD_BYTE(X86_OP_NEG_RM32);
    ADD_BYTE(0xD8); /* MOD R/M EAX /3 */

    ADD_BYTE(X86_OP_CMP_MEM32_WITH_REG);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);
    LOG_INSTR(instInfo.pX86Addr,count);

    if(DPREG_INFO.shiftAmt != 0){ 
      UNSUPPORTED;
    }
    UNSUPPORTED;
  }else{
    DP2("Immediate: Rn = %d, Rm = %d\n",DPREG_INFO.Rn, DPREG_INFO.Rm);

    ADD_BYTE(X86_OP_MOV_IMM_TO_EAX);
    ADD_WORD((uint32_t)DPIMM_INFO.imm * -1);
    
    if(DPIMM_INFO.rotate != 0){
      ADD_BYTE(X86_OP_ROR_RM32);
      ADD_BYTE(0xC8); /* MOD R/M EAX, /1 */
      ADD_BYTE(DPIMM_INFO.rotate * 2);
      DP1("Rotating by %d\n",DPIMM_INFO.rotate * 2);
    }

    ADD_BYTE(X86_OP_CMP_MEM32_WITH_REG);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uint32_t)&regFile[DPREG_INFO.Rn]);
    DP1("Comparing with 0x%x\n",((uint32_t)DPIMM_INFO.imm * -1));

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
orrHandler(void *pInst){
  struct decodeInfo_t instInfo = *(struct decodeInfo_t*)pInst;
  uint8_t count = 0;

  DP_HI;

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

    ADD_BYTE(X86_OP_OR_MEM32_TO_EAX);
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

    ADD_BYTE(X86_OP_OR_MEM32_TO_EAX);
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

  DP_BYE;
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

    if(DPIMM_INFO.rotate != 0){
      ADD_BYTE(X86_OP_ROR_RM32);
      ADD_BYTE(0xC8); /* MOD R/M EAX, /1 */
      ADD_BYTE(DPIMM_INFO.rotate * 2);
      DP1("Rotating by %d\n",DPIMM_INFO.rotate * 2);
    }

    ADD_BYTE(X86_OP_MOV_FROM_EAX);
    ADD_WORD((uintptr_t)&regFile[DPIMM_INFO.Rd]);
    LOG_INSTR(instInfo.pX86Addr,count);
  }

  return count;
}

OPCODE_HANDLER_RETURN
bicHandler(void *pInst){
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

    ADD_BYTE(X86_OP_NOT_RM32);
    ADD_BYTE(0xD0); /* MOD R/M EAX /2 */

    ADD_BYTE(X86_OP_AND_MEM32_TO_EAX);
    ADD_BYTE(0x05); /* MODR/M */
    ADD_WORD((uintptr_t)&regFile[DPREG_INFO.Rn]);

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

    ADD_BYTE(X86_OP_NOT_RM32);
    ADD_BYTE(0xD0); /* MOD R/M EAX /2 */
    
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

