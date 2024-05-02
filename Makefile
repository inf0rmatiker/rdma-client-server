.PHONY: clean sockets

SOCKETS_SRC_DIR = ./src/sockets
SOCKETS_INCLUDE = ./src/sockets
CC=gcc
CFLAGS=-I$(SOCKETS_INCLUDE)
OBJ = $(SOCKETS_SRC_DIR)/socket_common.o $(SOCKETS_SRC_DIR)/socket_server.o
DEPS = $(SOCKETS_SRC_DIR)/socket_common.h

%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

sockets: $(SOCKETS_SRC_DIR)/socket_server.o
	$(CC) -o socket-server $(SOCKETS_SRC_DIR)/socket_server.o


clean:
	rm -rf $(SOCKETS_SRC_DIR)/*.o *-client *-server