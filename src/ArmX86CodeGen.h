#ifndef _ARMX86_CODEGEN_H
#define _ARMX86_CODEGEN_H

#include "uthash.h"

void *initX86Code(void *stat);
void *initArmStack(void *stat);

struct hash_struct
{
  void *key;			/* key field */
  void *value;			/* value */
  UT_hash_handle hh;		/* makes this structure hashable */
};

static struct hash_struct *translationCache = NULL;

int InsertItem(void *address, void *startBlockAddress);
void FreeHashTableMemory(void);
void* GetItem(void *address);

#endif /* _ARMX86_CODEGEN_H */
