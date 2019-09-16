#include <stdio.h>
#include <stdlib.h>

#include <errno.h>
#include <string.h>

#include "../dynamicstack.h"

int main(int argc, char *argv[])
{
	DynamicStack *ds;
	int i;
	size_t p;
	size_t p2;

	ds = dynamicstack_new(0, 4, 128);
	p = 0;

	if(!ds)
		fprintf(stderr, "DynamicStack's address is 0x%p", (void *) ds);

	while(p++, !(i = dynamicstack_push(&ds, (void *) p))) {
		fprintf(stderr, "Pushed %lu\n", p);
	}

	fprintf(stderr, "Unable to push more: %s\n", strerror(-i));

	while(--p, (p2 = (size_t) dynamicstack_pop(&ds)))
		fprintf(stderr, "Got %lu, expected %lu, %s\n", p2, p, 
			p2 == p ? "all good" : "something's obviously gone wrong here");

	dynamicstack_delete(ds);
	return EXIT_SUCCESS;
}