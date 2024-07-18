.PHONY: clean

# Default target if just "make" is specified
.DEFAULT_GOAL := all

# C compiler to use
CC=gcc

RDMA_INCLUDE=/usr/include
RDMA_LIB=rdmacm
RDMA_LIBDIR=/usr/lib64
RDMA_SRC_DIR=./src/rdma

IBVERBS_LIB=ibverbs

RDMA_BINARIES=rdma-client rdma-server
_RDMA_CLIENT_DEPS=rdma_client.c rdma_common.c rdma_common.h
RDMA_CLIENT_DEPS=$(patsubst %,$(RDMA_SRC_DIR)/%,$(_RDMA_CLIENT_DEPS))
_RDMA_SERVER_DEPS=rdma_server.c rdma_common.c rdma_common.h
RDMA_SERVER_DEPS=$(patsubst %,$(RDMA_SRC_DIR)/%,$(_RDMA_SERVER_DEPS))

SOCKETS_SRC_DIR=./src/sockets
SOCKETS_BINARIES=socket-server socket-client
_SOCKETS_SERVER_DEPS=socket_server.c socket_common.c socket_common.h
SOCKETS_SERVER_DEPS=$(patsubst %,$(SOCKETS_SRC_DIR)/%,$(_SOCKETS_SERVER_DEPS))

_SOCKETS_CLIENT_DEPS=socket_client.c socket_common.c socket_common.h
SOCKETS_CLIENT_DEPS=$(patsubst %,$(SOCKETS_SRC_DIR)/%,$(_SOCKETS_CLIENT_DEPS))

# Socket targets
socket-server: $(SOCKETS_SERVER_DEPS)
	$(CC) -o $@ $^

socket-client: $(SOCKETS_CLIENT_DEPS)
	$(CC) -o $@ $^

# RDMA targets
rdma-server: $(RDMA_SERVER_DEPS)
	$(CC) -o $@ $^ -l$(RDMA_LIB) -l$(IBVERBS_LIB) -L$(RDMA_LIBDIR) -I$(RDMA_INCLUDE)

rdma-client: $(RDMA_CLIENT_DEPS)
	$(CC) -o $@ $^ -l$(RDMA_LIB) -l$(IBVERBS_LIB) -L$(RDMA_LIBDIR) -I$(RDMA_INCLUDE)

# Default/utility targets
all: $(SOCKETS_BINARIES) $(RDMA_BINARIES)

clean:
	rm -rvf $(SOCKETS_SRC_DIR)/*.o $(RDMA_SRC_DIR)/*.o $(SOCKETS_BINARIES) $(RDMA_BINARIES)