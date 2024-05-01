.PHONY: clean

socket-client:
	gcc -o socket-client src/socket_client.c

socket-server:
	gcc -o socket-server src/socket_server.c

clean:
	rm -rf *.o *-client *-server