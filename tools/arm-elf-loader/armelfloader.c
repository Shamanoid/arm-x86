#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <assert.h>
#include <string.h>

#define EI_NIDENT 	16
#define ET_EXEC         2
#define EM_ARM          40

typedef struct elfHeader_s{
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
}elfHeader_t;

typedef struct programHeader_s{
  uint32_t           p_type;
  uint32_t           p_offset;
  uint32_t           p_vaddr;
  uint32_t           p_paddr;
  uint32_t           p_filesz;
  uint32_t           p_memsz;
  uint32_t           p_flags;
  uint32_t           p_align;
}programHeader_t;

#ifdef DEBUG
#define DP1(x,y) printf(x,y)
#else
#define DP1(x,y)
#endif

#define READ_ELF32_HALF(elem,elf) { 	int numBytes; 					\
					numBytes = fread((elem),1,2,elf);		\
                                        if(numBytes < 2){				\
                                          fprintf(stderr,"End of File reached\n");	\
                                        }						\
                                  }							\

#define READ_ELF32_WORD(elem,elf) { 	int numBytes;                                   \
					numBytes = fread((elem),1,4,elf);		\
                                        if(numBytes < 4){				\
                                          fprintf(stderr,"End of File reached\n");	\
                                        }						\
                                  }							\

void parseElfHeader(FILE *elf, elfHeader_t *elfHeader){
  int numBytesRead;

  numBytesRead = fread(elfHeader->e_ident, 1, EI_NIDENT, elf);
  READ_ELF32_HALF(&elfHeader->e_type, elf)
  READ_ELF32_HALF(&elfHeader->e_machine, elf)
  READ_ELF32_WORD(&elfHeader->e_version, elf)
  READ_ELF32_WORD(&elfHeader->e_entry, elf)
  READ_ELF32_WORD(&elfHeader->e_phoff, elf)
  READ_ELF32_WORD(&elfHeader->e_shoff, elf)
  READ_ELF32_WORD(&elfHeader->e_flags, elf)
  READ_ELF32_HALF(&elfHeader->e_ehsize, elf)
  READ_ELF32_HALF(&elfHeader->e_phentsize, elf)
  READ_ELF32_HALF(&elfHeader->e_phnum, elf)
  READ_ELF32_HALF(&elfHeader->e_shentsize, elf)
  READ_ELF32_HALF(&elfHeader->e_shnum, elf)
  READ_ELF32_HALF(&elfHeader->e_shstrndx, elf)

  DP1("Elf Header Identification: %s\n",elfHeader->e_ident);
  DP1("ELF Type: %d\n",elfHeader->e_type);
  DP1("ELF Machine: %d\n",elfHeader->e_machine);
  DP1("ELF Version: %d\n",elfHeader->e_version);
  DP1("ELF Entry: %d\n",elfHeader->e_entry);
  DP1("ELF Program Header Offset: 0x%x\n",elfHeader->e_phoff);
  DP1("ELF Section Header Offset: 0x%x\n",elfHeader->e_shoff);
  DP1("ELF Flags: 0x%x\n",elfHeader->e_flags);
  DP1("ELF Header Size: %d\n",elfHeader->e_ehsize);
  DP1("ELF Program Header Ent Size: %d\n",elfHeader->e_phentsize);
  DP1("ELF Program Header Num: %d\n",elfHeader->e_phnum);
  DP1("ELF Section Header Ent Size: %d\n",elfHeader->e_shentsize);
  DP1("ELF Section Header Num: %d\n",elfHeader->e_shnum);
  DP1("ELF Section Header Str Index: %d\n",elfHeader->e_shstrndx);
}

void parseProgramHeader(FILE *elf, programHeader_t *programHeader){
  int numBytesRead;

  READ_ELF32_WORD(&programHeader->p_type, elf)
  READ_ELF32_WORD(&programHeader->p_offset, elf)
  READ_ELF32_WORD(&programHeader->p_vaddr, elf)
  READ_ELF32_WORD(&programHeader->p_paddr, elf)
  READ_ELF32_WORD(&programHeader->p_filesz, elf)
  READ_ELF32_WORD(&programHeader->p_memsz, elf)
  READ_ELF32_WORD(&programHeader->p_flags, elf)
  READ_ELF32_WORD(&programHeader->p_align, elf)

  DP1("Program Header Type: %d\n",programHeader->p_type);
  DP1("Program Segment Offset: %d\n",programHeader->p_offset);
  DP1("Program Virtual Address: 0x%x\n",programHeader->p_vaddr);
  DP1("Program Physical Address: 0x%x\n",programHeader->p_paddr);
  DP1("Program Segment File Image Size: %d\n",programHeader->p_filesz);
  DP1("Program Segment Memory Image Size: %d\n",programHeader->p_memsz);
  DP1("Program Header Flags: 0x%x\n",programHeader->p_flags);
  DP1("Program Header Alignment: 0x%x\n",programHeader->p_align);
}

int main(int argc, char *argv[]){
  elfHeader_t elfHeader;
  FILE *elf, *bin;
  uint32_t numBytesRead, i;

  if(argc != 2){
    printf("Please specify exactly one input elf file\n");
    exit(-1);
  }

  if((elf = fopen(argv[1],"rb")) == NULL){
    printf("Could not open %s\n", argv[1]);
    exit(-2);
  }

  //
  // FIXME
  // The file name should match the input file excepting
  // for the extension.
  //
  char *outputFileName = (char *)malloc(strlen(argv[1]) + 4);
  char *ptr;
  if(ptr = strrchr(argv[1], '.')) *ptr = '\0';
  sprintf(outputFileName,"%s.img",argv[1]);
  if((bin = fopen(outputFileName,"wb")) == NULL){
    printf("Could not open file for writing\n");
    exit(-3);
  }

  parseElfHeader(elf,&elfHeader);

  if(elfHeader.e_type != ET_EXEC){
    printf("Invalid ELF: Non-executable image\n");
    exit(-4);
  }

  if(elfHeader.e_machine != EM_ARM){
    printf("Invalid ELF: Not ARM executable\n");
    exit(-5);
  }

  if(elfHeader.e_phoff == 0){
    printf("Invalid ELF: Process image cannot be created\n");
    exit(-6);
  }

  for(i=0; i<elfHeader.e_phnum; i++){
    programHeader_t *programHeader = (programHeader_t *)malloc(sizeof(programHeader_t));
    parseProgramHeader(elf, programHeader);

    uint32_t inst;
    fseek(elf, programHeader->p_paddr, SEEK_SET);
    for(i=0; i<programHeader->p_filesz; i++){
      fread(&inst,1,4,elf);
      fwrite(&inst,1,4,bin);
    }
  }

  fclose(elf);
  fclose(bin);
}
