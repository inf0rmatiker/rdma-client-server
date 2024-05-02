.PHONY: clean sockets

SOCKETS_SRC_DIR = src/sockets
SOCKETS_INCLUDE = src/sockets
CC=gcc
CFLAGS=-I$(SOCKETS_INCLUDE)

sockets: $(SOCKETS_OBJ)
	$(CC) -o socket-server $(SOCKETS_SRC_DIR)/socket_server.c
	$(CC) -o socket-client $(SOCKETS_SRC_DIR)/socket_client.c


clean:
	rm -rf $(SOCKETS_SRC_DIR)/*.o *-client *-server