.PHONY: clean

client:
	gcc -o client src/socket_client.c

server:
	gcc -o server src/socket_server.c

clean:
	rm -rf *.o client server