#include <stdint.h>
#include <stdlib.h>
#include "codeenv.h"

struct hash_struct *translationCache = NULL;

#define X86_CODE_SIZE           0x2000000

void* initX86Code(void *stat){
  //
  // Allocate space for x86 instructions.
  // FIXME: Allocate space for x86 code based on an educated guess that
  // maps ARM code size to average x86 code size (include some give)
  //
  return malloc((size_t)X86_CODE_SIZE * sizeof(uint8_t));
}

#define ARM_STACK_SIZE          0x8000000
void* initArmStack(void *stat){
  uint8_t* armStackPtr;
  armStackPtr = (uint8_t *)malloc((size_t)ARM_STACK_SIZE * sizeof(uint8_t));
  return (void *)((armStackPtr == NULL)?NULL:(armStackPtr + ARM_STACK_SIZE));
}

int InsertItem(void *blockAddress, void *translatedAddress)
{
  struct hash_struct *s;

  /* allocate memory for hash table */
  s = malloc(sizeof(struct hash_struct));  

  /* memory allocation did not succeed */
  if(s == NULL)
    return -1;

  /* set the key and value pairs */
  s->key = blockAddress;
  s->value = translatedAddress;

  /* insert into hash table */
  HASH_ADD(hh, translationCache, key, sizeof(void *), s);

  return 0;
}

void FreeHashTableMemory(void)
{
  struct hash_struct *s;

  for(s = translationCache; s != NULL; s = s->hh.next)
    free(s);
}

void* GetItem(void *address)
{
  struct hash_struct *s;

  /* find key in hash */
  HASH_FIND(hh, translationCache, &address, sizeof(void *), s);

  /* return NULL if not exists or value if exists */
  if(s == NULL)
    return NULL;
  else
    return s->value;
}

