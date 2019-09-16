#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <limits.h>
#include <string.h>

#include <errno.h>
#include <pthread.h>

#include "queue.h"

struct queue {
	pthread_mutex_t lock;
	pthread_cond_t cond;

	int defunct;

	size_t count;
	size_t read;
	size_t written;

	size_t length;
	void *ptr[];
};

Queue * queue_new(size_t length)
{
	Queue *q;

	/* FIXME: one buffer is wasted by design */
	if (length < 1 ||
			SIZE_MAX < sizeof (Queue) ||
			(SIZE_MAX - sizeof (Queue)) / sizeof (void *) < 1 ||
			(SIZE_MAX - sizeof (Queue)) / sizeof (void *) - 1 < length ||
			!(q = malloc(sizeof (Queue) + sizeof (void *) * ++length))) 
		return NULL;

	if (pthread_mutex_init(&q->lock, NULL)) {
		free(q);
		return NULL;
	}

	if (pthread_cond_init(&q->cond, NULL)) {
		pthread_mutex_destroy(&q->lock);
		free(q);
		return NULL;
	}

	q->defunct = q->count = q->read = q->written = 0;
	q->length = length;

	return q;
}

int queue_finalize(Queue *q)
{
	int ret = 0;

	if (q == NULL)
		return -EINVAL;

	if ((ret = pthread_mutex_lock(&q->lock)))
		return -ret;

	q->defunct = 1;
	pthread_cond_broadcast(&q->cond);

	pthread_mutex_unlock(&q->lock);
	return ret;
}

int queue_delete(Queue *q)
{
	int ret = 0;

	if (q == NULL)
		return -EINVAL;

	if((ret = pthread_mutex_destroy(&q->lock)))
		return -ret;

	if((ret = pthread_cond_destroy(&q->cond)))
		return -ret;

	free(q);
	return ret;
}

int queue_enqueue(Queue *q, void **src, size_t min, size_t max)
{
	int ret;
	size_t s;

	if (q == NULL || src == NULL)
		return -EINVAL;

	if ((ret = pthread_mutex_lock(&q->lock)))
		return -ret;

	if (q->defunct) {
		ret = -EPIPE;
		goto cleanup;
	}

	/* FIXME: this can be optimized */
	for (s = 0; *src && s < max; s++) {
		while (q->count == q->length)
			if (s < min && !q->defunct)
				pthread_cond_wait(&q->cond, &q->lock);
			else
				goto cleanup_loop;

		q->ptr[q->written] = src[s];
		q->written = (q->written + 1) % q->length;
		q->count++;

		pthread_cond_signal(&q->cond);
	}

cleanup_loop:
	ret = s < INT_MAX ? s : INT_MAX;

cleanup:
	pthread_mutex_unlock(&q->lock);
	return ret;
}

int queue_dequeue(Queue *q, void **dest, size_t min, size_t max)
{
	int ret;
	size_t s;

	if (q == NULL || dest == NULL)
		return -EINVAL;

	if ((ret = pthread_mutex_lock(&q->lock)))
		return -ret;

	if (q->defunct && !q->count) {
		ret = -EPIPE;
		goto cleanup;
	}

	/* FIXME: this can be optimized */
	for (s = 0; s < max; s++) {
		while (!q->count)
			if (s < min && !q->defunct)
				pthread_cond_wait(&q->cond, &q->lock);
			else
				goto cleanup_loop;

		dest[s] = q->ptr[q->read]; 
		q->read = (q->read + 1) % q->length;
		q->count--;

		pthread_cond_signal(&q->cond);
	}

cleanup_loop:
	ret = s < INT_MAX ? s : INT_MAX;

cleanup:
	pthread_mutex_unlock(&q->lock);
	return ret;
}
