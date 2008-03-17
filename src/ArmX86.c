#include <stdio.h>
#include <stdlib.h>

#include "ArmX86Decode.h"
#include "ArmX86Debug.h"
#include "ArmX86Types.h"
#include "ArmX86ElfLoad.h"
#include "ArmX86CodeGen.h"

void printUsage(void);

int main(int argc, char *argv[]){
  uint32_t *pArmInstr;
  uint8_t *pX86Instr;

  /*
  // From the list of arguments, the first is taken to be the
  // ARM executable and the remaining are taken to be command
  // line arguments meant for the ARM executable. It follows
  // that there must be at least one argument to any run of
  // the binary translator.
  */
  if(argc == 1) {
    printUsage();
    exit(0);
  }

  if((pArmInstr = armX86ElfLoad(argv[1])) == NULL){
    DP_ASSERT(0,"Exiting..");
    exit(-1);
  }

  if((pX86Instr = (uint8_t *)initX86Code(NULL)) == NULL){
    DP_ASSERT(0,"Unable to create space for x86 code\n");
    exit(-1);
  }

  armX86Decode(pArmInstr, pX86Instr);

  return 0;
}

void printUsage(void){
  printf("Usage arm <arm-exe> <arm-exe-arg1> <arm-exe-arg2>...\n");
}
