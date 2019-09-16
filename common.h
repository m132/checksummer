#ifndef _COMMON_H
#define _COMMON_H

#include "queue.h"

#define ERR_REGULAR(cond, retval, label, text, ...) \
	if (cond) { \
		fprintf(stderr, "Error: " ERR_PREFIX_FMT text "\n", ERR_PREFIX __VA_ARGS__);  \
		ret = (retval) ? (retval) : -1; \
		goto label; \
	}

#define ERR ERR_REGULAR

#define PERR(cond, label, text, ...) \
	ERR(cond, -errno, label, text ": %s", __VA_ARGS__, strerror(errno))

/* FIXME: calculate this based on thread count */
#define NUM_BUFFERS 256

struct thread_context {
	int code;
	size_t type;
	size_t number;
	pthread_t ref;
	Queue *controller;
	Queue *spare;
	Queue *input;
	Queue *output;
	void *extra;
};

#endif /* _COMMON_H */
