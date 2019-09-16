#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include <errno.h>

#include "pathstack.h"

struct pathstack {
	size_t tip;
	size_t size;
	char ptr[];
};

PathStack* pathstack_new(char *path, size_t size)
{
	PathStack *p;

	if (size < 1 ||
			(SIZE_MAX - sizeof (PathStack)) / sizeof (char) < size ||
			!(p = malloc(sizeof (PathStack) + size * sizeof (char))))
		return NULL;

	p->tip = strlen(path);

	if (p->tip > size) {
		free(p);
		return NULL;
	}

	p->size = size;
	p->ptr[size - 1] = '\0';
	strcpy(p->ptr, path);

	return p;
}

void pathstack_delete(PathStack *p)
{
	free(p);
}

char* pathstack_path(PathStack *p)
{
	if (!p)
		return NULL;

	return p->ptr;
}

size_t pathstack_length(PathStack *p)
{
	if (!p)
		return 0;

	return p->tip;
}

size_t pathstack_size(PathStack *p)
{
	if (!p)
		return 0;

	return p->size;
}

int pathstack_push(PathStack *p, char *element)
{
	size_t length = strlen(element);

	if (!p || !length)
		return -EINVAL;

	if (p->size - p->tip < 2 || p->size - p->tip - 2 < length)
		return -ENOMEM; /* TODO: dynamic sizing */

	p->ptr[p->tip++] = '/';
	strcpy(&p->ptr[p->tip], element);
	p->tip += length;

	return 0;
}

int pathstack_pop(PathStack *p)
{
	if (!p)
		return -EINVAL;

	if (!p->tip)
		return -1;

	p->tip = (size_t) strrchr(p->ptr, '/');
	if (p->tip) /* TODO: tidy this up */
		p->tip -= (size_t) p->ptr;
	p->ptr[p->tip] = '\0';

	return 0;
}
