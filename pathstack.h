#ifndef _PATHSTACK_H
#define _PATHSTACK_H

typedef struct pathstack PathStack;

PathStack* pathstack_new(char *path, size_t size);
void pathstack_delete(PathStack *p);
char* pathstack_path(PathStack *p);
size_t pathstack_length(PathStack *p);
size_t pathstack_size(PathStack *p);
int pathstack_push(PathStack *p, char *element);
int pathstack_pop(PathStack *p);

#endif /* _PATHSTACK_H */
