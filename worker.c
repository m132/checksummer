#define ERR_PREFIX_FMT "Worker #%lu: "
#define ERR_PREFIX ctx->number,

#include "worker.h"

#include <openssl/evp.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const EVP_MD *digest;

struct worker_context {
	EVP_MD_CTX *md;
	size_t md_size;
	size_t buf_size;
	unsigned char *file_buffer;
};

static int process(struct worker_context *wctx, char *path, unsigned char *checksum)
{
	FILE *file;
	size_t read_bytes;

	file = fopen(path, "rb");
	if (!file) {
		fprintf(stderr,
			"Warning: Failed to calculate the checksum for %s: %s\n",
			path, strerror(errno));
		return -1;		
	}

	EVP_DigestInit_ex(wctx->md, digest, NULL);

	while ((read_bytes = fread(wctx->file_buffer, 1, wctx->buf_size, file)))
		EVP_DigestUpdate(wctx->md, wctx->file_buffer, read_bytes);

	if (ferror(file)) {
		fprintf(stderr,
			"Warning: Failed to calculate the checksum for %s: %s\n",
			path, strerror(ferror(file)));
		fclose(file);
		return -1;
	}

	fclose(file);
	EVP_DigestFinal_ex(wctx->md, checksum, NULL);
	return 0;
}

struct thread_context * worker_entry(struct thread_context *ctx)
{
	int ret;
	size_t s;

	struct worker_context wctx = {
		.md_size = EVP_MD_size(digest),
		.buf_size = EVP_MD_block_size(digest) * 1024
	};

	int dequeued;
	unsigned char *message[1];

	ret = 0;
	
	wctx.file_buffer = malloc(wctx.buf_size);
	PERR(!wctx.file_buffer, cleanup, "%s", "Failed to allocate file buffer")

	wctx.md = EVP_MD_CTX_new();
	PERR(!wctx.md, cleanup_file_buffer,
		"%s", "Failed to allocate message digest context")

	queue_enqueue(ctx->controller, (void **) &ctx, 1, 1);

	while ((dequeued = queue_dequeue(ctx->input, (void **) message, 1, 1)) > 0)
		for (s = dequeued; s--;)
			if (!process(&wctx, (char *) &message[s][wctx.md_size], &message[s][0]))
				queue_enqueue(ctx->output, (void **) &message[s], 1, 1);
			else
				queue_enqueue(ctx->spare, (void **) &message[s], 1, 1);

	EVP_MD_CTX_free(wctx.md);

cleanup_file_buffer:
	free(wctx.file_buffer);

cleanup:
	ctx->code = ret;
	queue_enqueue(ctx->controller, (void **) &ctx, 1, 1);

	return ctx;
}
