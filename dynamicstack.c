#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include <errno.h>

#include "dynamicstack.h"

struct dynamicstack {
	size_t unit; /* in word sizes */
	size_t size; /* in units */
	size_t min;
	size_t max;

	size_t tip;
	void *ptr[];
};

DynamicStack* dynamicstack_new(size_t min, size_t max, size_t unit)
{
	DynamicStack *ret;
	size_t size;

	if (max == 0)
		max = min;

	size = min ? min : 1;

	if (max < 1 || unit < 1 || max < min || 
			(SIZE_MAX - sizeof (DynamicStack)) / sizeof (void*) / unit < max ||
			!(ret = malloc(sizeof (DynamicStack) + size * unit * sizeof (void*))))
		return NULL;

	ret->tip = 0;
	ret->min = min;
	ret->size = size;
	ret->max = max;
	ret->unit = unit;

	return ret;
}

void dynamicstack_delete(DynamicStack *ds)
{
	free(ds);
}

int dynamicstack_push(DynamicStack **ds, void *ptr)
{
	DynamicStack *reallocated;

	if (!ds || !*ds || !ptr)
		return -EINVAL;

	assert((*ds)->size * (*ds)->unit >= (*ds)->tip);

	if ((*ds)->size * (*ds)->unit == (*ds)->tip) {
		if ((*ds)->max <= (*ds)->size)
			return -ENOSPC;

		reallocated = realloc(*ds, sizeof (DynamicStack) + 
			((*ds)->size + 1) * (*ds)->unit * sizeof (void*));

		if (!reallocated)
			return -ENOMEM;

		*ds = reallocated;
		(*ds)->size++;
	}

	(*ds)->ptr[(*ds)->tip++] = ptr;
	return 0;
}

void* dynamicstack_pop(DynamicStack **ds)
{
	DynamicStack *reallocated;

	if (!ds || !*ds || !(*ds)->tip)
		return NULL;

	(*ds)->tip--;

	if ((*ds)->size > (*ds)->min &&
			(*ds)->size * (*ds)->unit - (*ds)->tip > (*ds)->unit &&
			((*ds)->size - 1) * (*ds)->unit - (*ds)->tip > (*ds)->unit / 2) {

		reallocated = realloc(*ds, sizeof (DynamicStack) + 
			((*ds)->size - 1) * (*ds)->unit * sizeof (void*));

		if (reallocated) {
			*ds = reallocated;
			(*ds)->size--;
		}
	}

	return (*ds)->ptr[(*ds)->tip];
}