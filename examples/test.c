#include <stdio.h>
#include <stdlib.h>
#include "uthash.h"

struct hash_struct
{
  void *ptr;			/* key field */
  void *value;			/* value */
  UT_hash_handle hh;		/* makes this structure hashable */
};

struct hash_struct *translationCache = NULL;

int InsertItem(void *address, void *startBlockAddress)
{
  struct hash_struct *s;

  /* allocate memory for hash table */
  s = malloc(sizeof(struct hash_struct));  

  /* memory allocation did not succeed */
  if(s == NULL)
     return -1;

  /* set the key and value pairs */
  s->ptr = address;
  s->value = startBlockAddress;

  /* insert into hash table */
  HASH_ADD(hh, translationCache, ptr, sizeof(void *), s);

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

  /* return value */
  return s->value;
}

int main(void)
{
  printf("GetItem Address: %p\n", GetItem);
  printf("FreeHashTableMemory Address: %p\n", FreeHashTableMemory);
  printf("main Address: %p\n", main);
  printf("InsertItem Address: %p\n", InsertItem);

  InsertItem(GetItem, FreeHashTableMemory);
  InsertItem(main, InsertItem);

  printf("Value of key GetItem: %p\n", GetItem(GetItem));
  printf("Value of key main: %p\n", GetItem(main));

  FreeHashTableMemory();

  return 0;
}

