#define _GNU_SOURCE /* FIXME */

#define ERR_PREFIX_FMT 
#define ERR_PREFIX

#include "collector.h"
#include "common.h"
#include "crawler.h"
#include "dynamicstack.h"
#include "pathstack.h"
#include "queue.h"
#include "worker.h"

#include <mysql.h>
#include <openssl/evp.h>

#include <unistd.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum {
	QUEUE_SPARE,
	QUEUE_CONTROLLER,
	QUEUE_CRAWLER,
	QUEUE_WORKER,
	QUEUE_COLLECTOR
};

const EVP_MD *digest;

static int usage(char *name, int ret)
{
	fprintf(stderr, "Usage: %s [OPTION]... <INPUT PATH>...\n\n%s", name,
		"Calculates the checksums of all readable files found under\n"
		"INPUT PATHs, optionally storing them in a MySQL database.\n"
		"\n"
		"Options:\n"
		"    -c THREADS      amount of crawler threads to use\n"
		"    -w THREADS      amount of worker threads to use\n"
		"    -s THREADS      amount of collector threads to use\n"
		"    -d DIGEST       message digest to use\n"
		"    -h HOSTNAME     MySQL server hostname\n"
		"    -P PORT         MySQL server port\n"
		"    -D DATABASE     MySQL database to store results in\n"
		"    -q QUERY        MySQL query to use\n"
		"    -u USER         MySQL user name\n"
		"    -p              ask for MySQL user password\n");
	return ret;
}

int main(int argc, char *argv[])
{
	int ret, opt;
	size_t s, t;

	PathStack *p;
	Queue *q[5];
	unsigned char *msg_buffer;

	struct thread_context *ctx;
	struct mysql_connection_info con_info = {
		.port = 3389,
		.db = "checksummer",
		.user = "checksummer",
		.query = "INSERT INTO `checksums` (checksum, path) VALUES (?,?)"
	};

	struct {
		const char *name;
		const char *template;
		struct thread_context *(*entry_point)(struct thread_context *);
		size_t input_queue;
		size_t output_queue;
		void *extra;
		size_t started;
		size_t count;
	} threads[] = {
		{ "crawler"  , "Crawler #%lu"  , crawler_entry  , QUEUE_CRAWLER  , QUEUE_WORKER   , NULL     , 0, 1 },
		{ "worker"   , "Worker #%lu"   , worker_entry   , QUEUE_WORKER   , QUEUE_COLLECTOR, NULL     , 0, sysconf(_SC_NPROCESSORS_ONLN) },
		{ "collector", "Collector #%lu", collector_entry, QUEUE_COLLECTOR, QUEUE_SPARE    , &con_info, 0, 1 }
	};

	ret = 0;
	/* parse arguments */
	digest = EVP_sha256();

	while ((opt = getopt(argc, argv, "c:w:s:d:h:P:D:u:q:p")) != -1)
		switch (opt) {
			case 'c':
				threads[0].count = strtoul(optarg, NULL, 0); /* TODO: ERANGE */
				break;
			case 'w':
				threads[1].count = strtoul(optarg, NULL, 0); /* TODO: ERANGE */
				break;
			case 's':
				threads[2].count = strtoul(optarg, NULL, 0); /* TODO: ERANGE */
				break;
			case 'd':
				digest = EVP_get_digestbyname(optarg);
				break;
			case 'h':
				con_info.host = optarg;
				break;
			case 'P':
				con_info.port = strtoul(optarg, NULL, 0); /* TODO: ERANGE */
				break;
			case 'D':
				con_info.db = optarg;
				break;
			case 'u':
				con_info.user = optarg;
				break;
			case 'q':
				con_info.query = optarg;
				break;
			case 'p':
				/* TODO */
				break;
			default:
				return usage(argv[0], EXIT_FAILURE);
		}

	if (!digest) {
		fputs("Error: Invalid message digest specified\n", stderr);
		return EXIT_FAILURE;
	}

	for (s = 0; s < 3; ++s) {
		if (threads[s].count)
			continue;

		fprintf(stderr, "Error: Invalid %s thread count specified\n", threads[s].name);
		return EXIT_FAILURE;
	}

	if (optind == argc)
		return usage(argv[0], EXIT_SUCCESS);

	ret = mysql_library_init(argc, argv, NULL);
	ERR(ret, ret, cleanup, "Failed to initialize MySQL client library: %d", ret)

	/* construct shared data */
	for (s = 0; s < sizeof (q) / sizeof (Queue*); ++s) {
		q[s] = queue_new(NUM_BUFFERS);
		PERR(!q[s], cleanup_queue, "%s", "Queue allocation failed")
	}
	
	msg_buffer = malloc((EVP_MD_size(digest) + PATH_MAX) * NUM_BUFFERS);
	PERR(!msg_buffer, cleanup_queue,
		"%s", "Failed to allocate message buffers")

	{
		unsigned char *msg_buffers[NUM_BUFFERS];

		for(s = 0; s < NUM_BUFFERS; ++s)
			msg_buffers[s] = &msg_buffer[(EVP_MD_size(digest) + PATH_MAX) * s];

		queue_enqueue(q[QUEUE_SPARE],
			(void **) msg_buffers, NUM_BUFFERS, NUM_BUFFERS);
	}

	/* spawn the threads */
	{
#ifdef _GNU_SOURCE
		char name[16] = { '\0' };
#endif
		for(s = 0; s < 3; ++s)
			for (; threads[s].started < threads[s].count; ++threads[s].started) {
				ctx = malloc(sizeof (struct thread_context));
				if (!ctx) {
					fprintf(stderr, "Error: Failed to spawn %s thread #%lu: %s\n",
						threads[s].name, threads[s].started, strerror(errno));
					ret = -errno;
					goto init_wait;
				}

				ctx->code = 0;
				ctx->type = s;
				ctx->number = threads[s].started;
				ctx->controller = q[QUEUE_CONTROLLER];
				ctx->spare = q[QUEUE_SPARE];
				ctx->input = q[threads[s].input_queue];
				ctx->output = q[threads[s].output_queue];
				ctx->extra = threads[s].extra;

				ret = pthread_create(&ctx->ref,
					NULL, (void * (*)(void *)) threads[s].entry_point, ctx);
				if (ret) {
					fprintf(stderr, "Error: Failed to spawn %s thread #%lu: %s\n",
						threads[s].name, threads[s].started, strerror(ret));
					free(ctx);
					goto init_wait;
				}
#ifdef _GNU_SOURCE
				snprintf(name, 16, threads[s].template, threads[s].started);
				pthread_setname_np(ctx->ref, name);
#endif
			}
		s--;
	};
	/*
	 * wait for them to initialize, freeing the resources 
	 * of ones that fail to do so
	 */
init_wait:
	do {
		for (t = threads[s].started; t--;) {
			if(queue_dequeue(q[QUEUE_CONTROLLER], (void **) &ctx, 1, 1) != 1) {
				fputs("Fatal error: Controller queue corruption, aborting!",
					stderr);
				abort();
			}

			if (ctx->code) {
				if (pthread_join(ctx->ref, NULL))
					fprintf(stderr, "Warning: Failed to join %s thread #%lu\n",
						threads[s].name, t);

				threads[s].started--;
				ret = ctx->code;
				free(ctx);
			}
		}
	} while(s--);

	/* finalize queues without producers */
	for(s = 0; s < 3; ++s)
		if (!threads[s].started)
			queue_finalize(q[threads[s].output_queue]);

	/*
	 * ret is carried over from the spawn and init_wait loops.
	 * if it's set, an error has occured in either of these and
	 * no further initialization work should be done
	 */
	if (ret)
		goto cleanup_thread;

	/* construct pathstacks and enqueue them in crawler's queue */
	for (; optind < argc; ++optind) {
		p = pathstack_new(argv[optind], PATH_MAX);
		if (p) {
			queue_enqueue(q[QUEUE_CRAWLER], (void **) &p, 1, 1);
		} else
			fprintf(stderr, "Warning: Failed to construct the path stack: %s",
				strerror(errno));
	}

cleanup_thread:
	queue_finalize(q[QUEUE_CRAWLER]);

	/*
	 * join the threads, free their contexts and, if there are no other
	 * producers left, finalize their output queues 
	 *
	 * the loops are here just to run this part exactly <running thread count> 
	 * times, values of s and t shouldn't be used for any other purpose!
	 */
	for(s = 0; s < 3; ++s)
		for (t = threads[s].started; t--;) {
			if (queue_dequeue(q[QUEUE_CONTROLLER], (void **) &ctx, 1, 1) != 1) {
				fputs("Fatal error: Controller queue corruption, aborting!",
					stderr);
				abort();
			}

			if (pthread_join(ctx->ref, NULL))
				fprintf(stderr, "Warning: Failed to join %s thread #%lu\n",
				threads[ctx->type].name, ctx->number);
		
			if (!--threads[ctx->type].started)
				queue_finalize(q[threads[s].output_queue]);

			free(ctx);
		}

	free(msg_buffer);
	s = sizeof (q) / sizeof (Queue*);

cleanup_queue:
	while(s--)
		queue_delete(q[s]);

	mysql_library_end();

cleanup:
	return ret;
}
