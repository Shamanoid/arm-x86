#include <stdio.h>
#include <stdlib.h>

#include "ArmX86Decode.h"
#include "ArmX86Debug.h"
#include "ArmX86Types.h"
#include "ArmX86ElfLoad.h"

void printUsage(void);

int main(int argc, char *argv[]){
  uint32_t *instr;

  //
  // From the list of arguments, the first is taken to be the
  // ARM executable and the remaining are taken to be command
  // line arguments meant for the ARM executable. It follows
  // that there must be at least one argument to any run of
  // the binary translator.
  //
  if(argc == 1) {
    printUsage();
    exit(0);
  }

  if((instr = armX86ElfLoad(argv[1])) == NULL){
    DP_ASSERT(0,"Exiting..");
    exit(-1);
  }
  armX86Decode(instr);

  return 0;
}

void printUsage(void){
  printf("Usage arm <arm-exe> <arm-exe-arg1> <arm-exe-arg2>...\n");
}
