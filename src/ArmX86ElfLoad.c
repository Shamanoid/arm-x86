#include <stdint.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include "ArmX86Debug.h"
#include "ArmX86ElfLoad.h"

#define EI_NIDENT 	    16
#define ET_EXEC             2
#define EM_ARM              40

struct elfHeader_t{
    unsigned           char e_ident[EI_NIDENT];
    uint16_t           e_type;
    uint16_t           e_machine;
    uint32_t           e_version;
    uint32_t           e_entry;
    uint32_t           e_phoff;
    uint32_t           e_shoff;
    uint32_t           e_flags;
    uint16_t           e_ehsize;
    uint16_t           e_phentsize;
    uint16_t           e_phnum;
    uint16_t           e_shentsize;
    uint16_t           e_shnum;
    uint16_t           e_shstrndx;
};

struct programHeader_t{
    uint32_t           p_type;
    uint32_t           p_offset;
    uint32_t           p_vaddr;
    uint32_t           p_paddr;
    uint32_t           p_filesz;
    uint32_t           p_memsz;
    uint32_t           p_flags;
    uint32_t           p_align;
};

struct symTableEntry_t{
    uint32_t           st_name;
    uint32_t           st_value;
    uint32_t           st_size;
    unsigned char      st_info;
    unsigned char      st_other;
    uint16_t           st_shndx;
};

FILE *elf, *bin;

#define READ_ELF32_HALF(elem,elf)             \
    do {                                      \
        int numBytes;                         \
        numBytes = fread((elem),1,2,elf);     \
        if (numBytes < 2) {                   \
            DP("End of File reached\n");      \
        }                                     \
    } while(0)

#define READ_ELF32_WORD(elem,elf)             \
    do {                                      \
        int numBytes;                         \
        numBytes = fread((elem),1,4,elf);     \
        if (numBytes < 4) {                   \
            DP("End of File reached\n");      \
        }                                     \
    } while (0)

void parseElfHeader(FILE *elf, struct elfHeader_t *pElfHeader){
  int numBytesRead;

  numBytesRead = fread(pElfHeader->e_ident, 1, EI_NIDENT, elf);
  READ_ELF32_HALF(&pElfHeader->e_type, elf);
  READ_ELF32_HALF(&pElfHeader->e_machine, elf);
  READ_ELF32_WORD(&pElfHeader->e_version, elf);
  READ_ELF32_WORD(&pElfHeader->e_entry, elf);
  READ_ELF32_WORD(&pElfHeader->e_phoff, elf);
  READ_ELF32_WORD(&pElfHeader->e_shoff, elf);
  READ_ELF32_WORD(&pElfHeader->e_flags, elf);
  READ_ELF32_HALF(&pElfHeader->e_ehsize, elf);
  READ_ELF32_HALF(&pElfHeader->e_phentsize, elf);
  READ_ELF32_HALF(&pElfHeader->e_phnum, elf);
  READ_ELF32_HALF(&pElfHeader->e_shentsize, elf);
  READ_ELF32_HALF(&pElfHeader->e_shnum, elf);
  READ_ELF32_HALF(&pElfHeader->e_shstrndx, elf);

  DP1("Elf Header Identification: %s\n",pElfHeader->e_ident);
  DP1("ELF Type: %d\n",pElfHeader->e_type);
  DP1("ELF Machine: %d\n",pElfHeader->e_machine);
  DP1("ELF Version: %d\n",pElfHeader->e_version);
  DP1("ELF Entry: %p\n",(void *)pElfHeader->e_entry);
  DP1("ELF Program Header Offset: 0x%x\n",pElfHeader->e_phoff);
  DP1("ELF Section Header Offset: 0x%x\n",pElfHeader->e_shoff);
  DP1("ELF Flags: 0x%x\n",pElfHeader->e_flags);
  DP1("ELF Header Size: %d\n",pElfHeader->e_ehsize);
  DP1("ELF Program Header Ent Size: %d\n",pElfHeader->e_phentsize);
  DP1("ELF Program Header Num: %d\n",pElfHeader->e_phnum);
  DP1("ELF Section Header Ent Size: %d\n",pElfHeader->e_shentsize);
  DP1("ELF Section Header Num: %d\n",pElfHeader->e_shnum);
  DP1("ELF Section Header Str Index: %d\n",pElfHeader->e_shstrndx);
}

void parseProgramHeader(FILE *elf, struct programHeader_t *pProgramHeader){

  READ_ELF32_WORD(&pProgramHeader->p_type, elf);
  READ_ELF32_WORD(&pProgramHeader->p_offset, elf);
  READ_ELF32_WORD(&pProgramHeader->p_vaddr, elf);
  READ_ELF32_WORD(&pProgramHeader->p_paddr, elf);
  READ_ELF32_WORD(&pProgramHeader->p_filesz, elf);
  READ_ELF32_WORD(&pProgramHeader->p_memsz, elf);
  READ_ELF32_WORD(&pProgramHeader->p_flags, elf);
  READ_ELF32_WORD(&pProgramHeader->p_align, elf);

  DP1("Program Header Type: %d\n",pProgramHeader->p_type);
  DP1("Program Segment Offset: %d\n",pProgramHeader->p_offset);
  DP1("Program Virtual Address: 0x%x\n",pProgramHeader->p_vaddr);
  DP1("Program Physical Address: 0x%x\n",pProgramHeader->p_paddr);
  DP2("Program Segment File Image Size: %d, 0x%x\n",
    pProgramHeader->p_filesz,
    pProgramHeader->p_filesz
  );
  DP2("Program Segment Memory Image Size: %d, 0x%x\n",
    pProgramHeader->p_memsz,
    pProgramHeader->p_memsz
  );
  DP1("Program Header Flags: 0x%x\n",pProgramHeader->p_flags);
  DP1("Program Header Alignment: 0x%x\n",pProgramHeader->p_align);
}

uint32_t* armX86ElfLoad(char *elfFile){
  struct elfHeader_t elfHeader;
  uint32_t i, *instr = 0, j;
  int elfFd;

  DP_HI;

  if((elf = fopen(elfFile,"rb")) == NULL){
    DP1("Could not open %s\n", elfFile);
    return NULL;
  }

  char *outputFileName = (char *)malloc(strlen(elfFile) + 4);
  char *ptr;
  if((ptr = strrchr(elfFile, '.')) != NULL) *ptr = '\0';
  sprintf(outputFileName,"%s.img",elfFile);

  parseElfHeader(elf,&elfHeader);

  if(elfHeader.e_type != ET_EXEC){
    DP("Invalid ELF: Non-executable image\n");
    return NULL;
  }

  if(elfHeader.e_machine != EM_ARM){
    DP("Invalid ELF: Not ARM executable\n");
    return NULL;;
  }

  if(elfHeader.e_phoff == 0){
    DP("Invalid ELF: Process image cannot be created\n");
    return NULL;
  }

  /*
  // The current strategy for loading the elf file is as follows:
  // Create a dummy file that is mmap-ed to the desired starting address
  //   with the size being the sum of the sizes mem-sizes of all the
  //   segments. This is an approximation because the segments may
  //   be overlapping. The file is mmap-ed as read/write.
  // Populate the memory area which corresponds to the mmap-ed file from
  //   the source ARM binary.
  // This seems like an ungainly way of achieving the laoding function, but
  //   it works for now.
  */
  uint8_t *inst = 0;
  uint32_t numBytesRead = 0;
  struct programHeader_t *pProgramHeader =
      (struct programHeader_t *)malloc(sizeof(struct programHeader_t));

  uint32_t imageSize = 0;
  uint32_t imageLoc = 0;
  for(i=0; i<elfHeader.e_phnum; i++){
    fseek(elf, elfHeader.e_phoff + i * elfHeader.e_phentsize, SEEK_SET);
    parseProgramHeader(elf, pProgramHeader);
    imageLoc = 0x8000;
    imageSize += pProgramHeader->p_memsz;
  }

  if((bin = fopen(outputFileName,"wb")) == NULL){
    DP("Could not open file for writing\n");
    return NULL;
  }

  /*
  // FIXME:
  // This is BAD hack
  */
  imageSize = 0x100000;
  uint32_t temp = 0;
  for(i=0; i<imageSize; i+=4){
    fwrite(&temp,1,4,bin);
  }
  fclose(bin);

  if((elfFd = open(outputFileName,O_RDWR)) == -1){
    DP1("Unable to open image for loading: %x\n",errno);
    return NULL;
  }

  DP1("Image location is 0x%x.",imageLoc);
  instr = (uint32_t *)mmap(
    (caddr_t)imageLoc,
    imageSize,
    (PROT_READ | PROT_WRITE | PROT_EXEC),
    MAP_SHARED,
    elfFd,
    0
  );

  if(instr == (uint32_t *)-1){
    perror("Could not load image file to memory");
    return NULL;
  }
  DP1("Loaded image file at %p\n",instr);

  for(i=0; i<elfHeader.e_phnum; i++){
    fseek(elf, elfHeader.e_phoff + i * elfHeader.e_phentsize, SEEK_SET);
    parseProgramHeader(elf, pProgramHeader);

    inst = (uint8_t *)(pProgramHeader->p_vaddr);
    fseek(elf, pProgramHeader->p_offset, SEEK_SET);
    numBytesRead = fread((void *)inst,1,pProgramHeader->p_filesz,elf);

    if(pProgramHeader->p_filesz != 0){
      DP_ASSERT(numBytesRead != 0,"Encountered file read error\n");
    }

    for(j=pProgramHeader->p_filesz; j<pProgramHeader->p_memsz; j++){
      *(inst+j) = 0;
    }
  }

  if(close(elfFd) == -1){
    DP("Failed to close image file post-loading\n");
  }
  free(pProgramHeader);
  fclose(elf);
  free(outputFileName);  

  DP_BYE;
  return (uint32_t *)elfHeader.e_entry;
}
