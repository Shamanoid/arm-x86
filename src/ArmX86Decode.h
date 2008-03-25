#ifndef _ARMX86_DECODE_H
#define _ARMX86_DECODE_H

#include <stdint.h>
#include <assert.h>
#include "ArmX86Types.h"

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
      uint8_t cond;
      bool P;
      bool U;
      bool S;
      bool W;
      bool L;
      uint8_t Rn;
      uint16_t regList;
    }lsmult;

    struct {
      uint8_t cond;
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
      uint8_t cond;
      bool S;
      uint8_t Rn;
      uint8_t Rd;
      uint8_t rotate;
      uint8_t imm;
    }dpimm;

    struct {
      uint8_t cond;
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
      uint8_t cond;
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
      uint8_t cond;
      bool L;
      uint32_t offset;
    }branch;
  }armInstInfo;
  bool immediate; /* True => DPIMM */
  uint8_t* pX86Addr;
};

#define OPCODE_HANDLER_RETURN   int
void armX86Decode(const struct map_t *memMap);
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

#endif /* _ARMX86_DECODE_H */
