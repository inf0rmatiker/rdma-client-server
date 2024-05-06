.PHONY: clean sockets

# Default target if just "make" is specified
.DEFAULT_GOAL := all

CC=gcc

RDMA_INCLUDE=/usr/include
RDMA_LIB=rdmacm
RDMA_LIBDIR=/usr/lib64
RDMA_SRC_DIR=./src/rdma

SOCKETS_SRC_DIR=./src/sockets
SOCKETS_INCLUDE=./src/sockets
SOCKETS_CFLAGS=-I$(SOCKETS_INCLUDE)

_SOCKETS_SERVER_OBJ=socket_common.o socket_server.o
SOCKETS_SERVER_OBJ=$(patsubst %,$(SOCKETS_SRC_DIR)/%,$(_SOCKETS_SERVER_OBJ))

_SOCKETS_CLIENT_OBJ=socket_common.o socket_client.o
SOCKETS_CLIENT_OBJ=$(patsubst %,$(SOCKETS_SRC_DIR)/%,$(_SOCKETS_CLIENT_OBJ))

_SOCKETS_DEPS=socket_common.h
SOCKETS_DEPS=$(patsubst %,$(SOCKETS_SRC_DIR)/%,$(_SOCKETS_DEPS))

%.o: %.c $(SOCKETS_DEPS)
	$(CC) -c -o $@ $< $(SOCKETS_CFLAGS)

socket-server: $(SOCKETS_SERVER_OBJ)
	$(CC) -o $@ $^ $(SOCKETS_CFLAGS)


socket-client: $(SOCKETS_CLIENT_OBJ)
	$(CC) -o $@ $^ $(SOCKETS_CFLAGS)

rdma-server:
	$(CC) -o $@ -l$(RDMA_LIB) -L$(RDMA_LIBDIR) -I$(RDMA_INCLUDE) $(RDMA_SRC_DIR)/rdma_server.c

all: socket-server socket-client

clean:
	rm -rf $(SOCKETS_SRC_DIR)/*.o *-client *-server