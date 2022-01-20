#define ERR_PREFIX_FMT "Collector #%lu: "
#define ERR_PREFIX ctx->number,

#include "collector.h"

#include <mysql.h>
#include <openssl/evp.h>

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern const EVP_MD *digest;

static inline char* bin2hex(unsigned char *bin, char *hex, size_t length)
{
	while (length--)
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
		((uint16_t *) hex)[length] = 
			(((bin[length] & 0xf0) << 4) + 
			((bin[length] & 0xf0) < 0xa0 ? 0x3000 : 0x5700)) | 
			(((bin[length] & 0x0f)) + 
			((bin[length] & 0x0f) < 0x0a ? 0x30 : 0x57));
#else
		((uint16_t *) hex)[length] = 
			(((bin[length] & 0xf0) >> 4) +
			((bin[length] & 0xf0) < 0xa0 ? 0x30 : 0x57)) | 
			(((bin[length] & 0x0f) << 8) +
			((bin[length] & 0x0f) < 0x0a ? 0x3000 : 0x5700));
#endif

	return hex;
}

struct thread_context * collector_entry(struct thread_context *ctx)
{
	int ret = 0;

	size_t md_size = EVP_MD_size(digest);
	char hex_checksum[md_size * 2];

	int dequeued;
	unsigned char *message[NUM_BUFFERS];
	char *path[NUM_BUFFERS];
	size_t s, md_length[NUM_BUFFERS], path_length[NUM_BUFFERS];

	struct mysql_connection_info *con_info = ctx->extra;
	MYSQL *mysql;
	MYSQL_STMT *stmt;
	MYSQL_BIND param[2] = {
		{
			.buffer_type = MYSQL_TYPE_BLOB,
			.buffer = message,
			.length = md_length
		},
		{
			.buffer_type = MYSQL_TYPE_STRING,
			.buffer = path,
			.length = path_length
		}
	};
	char *host = NULL; /* FIXME */
	char *unix_socket = NULL;

	/* initialization */
	mysql = NULL;

	if (con_info->host) {
		ret = mysql_thread_init();
		ERR(ret, ret, cleanup,
			"%s", "Failed to initialize MySQL client library")

		mysql = mysql_init(NULL);
		ERR(!mysql, -ENOMEM, cleanup_mysql_thread,
			"%s", "Failed to initialize MySQL context")

		if (!strncmp("unix:", con_info->host, 5))
			unix_socket = &con_info->host[5];
		else
			host = con_info->host;

		ERR(!mysql_real_connect(mysql,
				host,
				con_info->user,
				con_info->passwd,
				con_info->db,
				con_info->port,
				unix_socket, 0),
			-ECONNREFUSED, cleanup_mysql,
			"Failed to connect to the MySQL server: %s", mysql_error(mysql))

		stmt = mysql_stmt_init(mysql);
		ERR(!stmt, -1, cleanup_mysql,
			"Failed create a prepared statement: %s", mysql_error(mysql))

		ret = mysql_stmt_prepare(stmt, con_info->query, -1);
		ERR(ret, -1, cleanup_stmt,
			"Failed prepare a MySQL statement: %s", mysql_stmt_error(stmt))

		ret = mysql_stmt_bind_param(stmt, param);
		ERR(ret, -1, cleanup_stmt,
			"Failed bind parameters to the MySQL statement: %s",
			mysql_stmt_error(stmt))
	}

	queue_enqueue(ctx->controller, (void **) &ctx, 1, 1);

	/* main loop */
	for (; (dequeued = queue_dequeue(ctx->input, (void **) message, 1, NUM_BUFFERS)) > 0;) {
		if (mysql) {
			ret = mysql_stmt_attr_set(stmt, STMT_ATTR_ARRAY_SIZE, &dequeued);
			if (ret) {
				fprintf(stderr,
					"Warning: Collector: Failed to set statement attributes: %s\n",
					mysql_stmt_error(stmt));
				queue_enqueue(ctx->spare, (void **) message, dequeued, dequeued);
				continue;
			}
		}

		for (s = 0; s < dequeued; ++s) {
			md_length[s] = md_size;
			path[s] = (char *) &message[s][md_length[s]];
			path_length[s] = strlen(path[s]);

			printf("%.*s  %s\n",
				(int) md_length[s] * 2,
				bin2hex(message[s], hex_checksum, md_length[s]),
				path[s]);
		}

		if (mysql && mysql_stmt_execute(stmt)) {
			fprintf(stderr, "Warning: Collector: INSERT failed: %s\n",
				mysql_stmt_error(stmt));
			queue_enqueue(ctx->spare, (void **) message, dequeued, dequeued);
		} else
			queue_enqueue(ctx->output, (void **) message, dequeued, dequeued);
	}

cleanup_stmt:
	mysql_stmt_close(stmt);

cleanup_mysql:
	mysql_close(mysql);

cleanup_mysql_thread:
	mysql_thread_end();

cleanup:
	ctx->code = ret;
	queue_enqueue(ctx->controller, (void **) &ctx, 1, 1);

	return ctx;
}
