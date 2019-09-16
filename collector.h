#ifndef _COLLECTOR_H
#define _COLLECTOR_H

#include "common.h"

struct mysql_connection_info {
	char *host;
	char *user;
	char *passwd;
	char *db;
	char *table;
	char *query;
	unsigned int port;
};

struct thread_context * collector_entry(struct thread_context *ctx);

#endif /* _COLLECTOR_H */
