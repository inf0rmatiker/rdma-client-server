.PHONY: clean sockets

# Default target if just "make" is specified
.DEFAULT_GOAL := all

SOCKETS_SRC_DIR=./src/sockets
SOCKETS_INCLUDE=./src/sockets
CC=gcc
CFLAGS=-I$(SOCKETS_INCLUDE)

_SOCKETS_SERVER_OBJ=socket_common.o socket_server.o
SOCKETS_SERVER_OBJ=$(patsubst %,$(SOCKETS_SRC_DIR)/%,$(_SOCKETS_SERVER_OBJ))

_SOCKETS_CLIENT_OBJ=socket_common.o socket_client.o
SOCKETS_CLIENT_OBJ=$(patsubst %,$(SOCKETS_SRC_DIR)/%,$(_SOCKETS_CLIENT_OBJ))

_SOCKETS_DEPS=socket_common.h
SOCKETS_DEPS=$(patsubst %,$(SOCKETS_SRC_DIR)/%,$(_SOCKETS_DEPS))

%.o: %.c $(SOCKETS_DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

socket-server: $(SOCKETS_SERVER_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)


socket-client: $(SOCKETS_CLIENT_OBJ)
	$(CC) -o $@ $^ $(CFLAGS)

all: socket-server socket-client

clean:
	rm -rf $(SOCKETS_SRC_DIR)/*.o *-client *-server