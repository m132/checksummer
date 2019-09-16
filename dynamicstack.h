#ifndef _DYNAMICSTACK_H
#define _DYNAMICSTACK_H

typedef struct dynamicstack DynamicStack;

DynamicStack* dynamicstack_new(size_t min, size_t max, size_t unit);
void dynamicstack_delete(DynamicStack *ds);
int dynamicstack_push(DynamicStack **ds, void *ptr);
void* dynamicstack_pop(DynamicStack **ds);

#endif /* _DYNAMICSTACK_H */
