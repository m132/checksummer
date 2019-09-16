#ifndef _QUEUE_H
#define _QUEUE_H

#include <pthread.h>
#include <linux/limits.h>

typedef struct queue Queue;

enum queue_usage {
	USAGE_CONSUMER,
	USAGE_PRODUCER
};

enum queue_blocking_mode {
	BLOCK_ALWAYS,
	BLOCK_LIKELY,
	BLOCK_NEVER
};

#define QUEUE_FLAG_RET_NO_PEERS 1 << 0

Queue* queue_new(size_t length);
int queue_finalize(Queue *q);
int queue_delete(Queue *q);
int queue_enqueue(Queue *q, void **src, size_t min, size_t max);
int queue_dequeue(Queue *q, void **dest, size_t min, size_t max);

#endif /* _QUEUE_H */
