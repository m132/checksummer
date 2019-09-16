CC ?= gcc
CFLAGS ?= -pipe -std=c99 -Wall -pedantic -O0 -g

OPENSSL_CFLAGS ?= `pkg-config --cflags openssl`
OPENSSL_LIBS ?= `pkg-config --libs openssl`
MARIADB_CFLAGS ?= `pkg-config --cflags libmariadb`
MARIADB_LIBS ?= `pkg-config --libs libmariadb`

OBJ = \
	pathstack.o \
	queue.o \
	dynamicstack.o \
	crawler.o \
	worker.o \
	collector.o \
	controller.o

.PHONY: clean

all: checksummer

tests: tests/dynamicstack_test

clean:
	rm *.o tests/*.o checksummer tests/dynamicstack_test

%.o: %.c
	$(CC) -pthread $(OPENSSL_CFLAGS) $(MARIADB_CFLAGS) $(CFLAGS) -o $@ -c $<

checksummer: $(OBJ)
	$(CC) -pthread $(OPENSSL_LIBS) $(MARIADB_LIBS) $(LIBS) -o $@ $^

tests/dynamicstack_test: dynamicstack.o tests/dynamicstack_test.o 
	$(CC) $(LIBS) -o $@ $^
