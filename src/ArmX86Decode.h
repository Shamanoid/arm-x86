#include <stdint.h>

void armX86Decode(uint32_t *instr);
void andHandler(void *inst);
void eorHandler(void *inst);
void subHandler(void *inst);
void rsbHandler(void *inst);
void addHandler(void *inst);
void adcHandler(void *inst);
void sbcHandler(void *inst);
void rscHandler(void *inst);
void tstHandler(void *inst);
void teqHandler(void *inst);
void cmpHandler(void *inst);
void cmnHandler(void *inst);
void orrHandler(void *inst);
void movHandler(void *inst);
void bicHandler(void *inst);
void mvnHandler(void *inst);
