#include <sys/queue.h>
#include <stdio.h>

#define INIT_LIST(x) \
	SLIST_HEAD(slisthead, entry) x = SLIST_HEAD_INITIALIZER(x); \
	SLIST_INIT(&x)
/* this inserts an item at the head of the list */
#define INSERT_ITEM(head, item) \
	SLIST_INSERT_HEAD(&head, &item, entries)
#define REMOVE_ITEM(head, item) \
	SLIST_REMOVE(&head, &item, entry, entries)
#define LOOP_THRU_LIST(itrPtr, head) \
	SLIST_FOREACH(itrPtr, &head, entries)

/* this is the structure needed for each linked list entry 
 * NOTE: "SLIST_ENTRY(entry) entries" is needed to link list entries together.
 * a: the data we want to store in each list
 */
struct entry
{
	int a;	
	SLIST_ENTRY(entry) entries;
};

int main(void)
{
	struct entry a, b, c, *np;
	a.a = 1; b.a = 2; c.a = 3;

	/* init list */
	INIT_LIST(head);

	/* insert 3 items into the list */
	INSERT_ITEM(head, a);
	INSERT_ITEM(head, b);
	INSERT_ITEM(head, c);

	printf("Initial linked list contents:\n");
	/* output the list items */
	LOOP_THRU_LIST(np, head)
	{
		printf("%d\n", np->a);
	}

	/* remove list item */
	REMOVE_ITEM(head, a);

	printf("Linked list contents with a(1) removed:\n");
	/* output the updated list items */
	LOOP_THRU_LIST(np, head)
	{
		printf("%d\n", np->a);
	}

	return 0;
}
