#define _GNU_SOURCE /* FIXME */

#define ERR_PREFIX_FMT "Crawler #%lu: "
#define ERR_PREFIX ctx->number,

#include "crawler.h"

#include "dynamicstack.h"
#include "pathstack.h"

#include <openssl/evp.h>

#include <dirent.h>

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const EVP_MD *digest;

static int crawl(struct thread_context *ctx, DynamicStack **ds, PathStack *p)
{
	size_t md_size = EVP_MD_size(digest);
	size_t path_size = PATH_MAX;

	DIR *dir, *temp;
	struct dirent *entry;

	unsigned char *message[1];

	dir = opendir(pathstack_path(p));
	if (!dir) {
		 fprintf(stderr, "Warning: Crawler #%lu: Failed to open %s: %s\n",
		 	ctx->number, pathstack_path(p), strerror(errno));
		return 0;
	}

	do {
		while ((entry = readdir(dir))) {
			if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
				continue;

			if (entry->d_type != DT_DIR && entry->d_type != DT_REG)
				continue;

			if (pathstack_push(p, entry->d_name) < 0) {
				fprintf(stderr, "Warning: Failed to crawl %s: %s\n",
					pathstack_path(p), strerror(ENAMETOOLONG));
				continue;
			}

			switch (entry->d_type) {
				case DT_DIR:
					temp = opendir(pathstack_path(p));

					if (!temp) {
						fprintf(stderr, "Warning: Failed to crawl %s: %s\n",
							pathstack_path(p), strerror(errno));
						pathstack_pop(p);
						continue;
					}
						
					if (dynamicstack_push(ds, dir) < 0) {
						closedir(temp);
						fprintf(stderr, "Warning: Failed to crawl %s: "
							"Directory structure too deep\n",
							pathstack_path(p));
						pathstack_pop(p);
						continue;
					}

					dir = temp;
					continue;
					break;
				case DT_REG:
					if (queue_dequeue(ctx->spare, (void **) message, 1, 1) < 1)
						return -1;

					strncpy((char *) &message[0][md_size], 
						pathstack_path(p), path_size - 1);
					queue_enqueue(ctx->output, (void **) message, 1, 1);
					break;
			}

			pathstack_pop(p);
		}

		pathstack_pop(p);
		closedir(dir);
	} while ((dir = dynamicstack_pop(ds)));

	return 0;
}

struct thread_context * crawler_entry(struct thread_context *ctx)
{
	int ret = 0;
	size_t s;

	DynamicStack *ds;

	PathStack *message[1];
	int dequeued;

	ds = dynamicstack_new(0, 8, 512);
	ERR(!ds, -ENOMEM, cleanup, "%s", "Dynamic stack allocation failed")

	queue_enqueue(ctx->controller, (void **) &ctx, 1, 1);

	while ((dequeued = queue_dequeue(ctx->input, (void **) message, 1, 1)) > 0)
		for(s = dequeued; s--; free(message[s]))
			if (crawl(ctx, &ds, message[s]))
				goto cleanup_ds;

cleanup_ds:
	dynamicstack_delete(ds);

cleanup:
	ctx->code = ret;
	queue_enqueue(ctx->controller, (void **) &ctx, 1, 1);

	return ctx;
}
