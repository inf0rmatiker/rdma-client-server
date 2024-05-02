#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "socket_common.h"

void print_usage() {
        printf("Usage:\n\t./socket-server <listen_port>\n");
        printf("Example:\n\t./socket-server 8082\n");
}

int main(int argc, char** argv) {

        if (argc < 2) {
                print_usage();
                return 1;
        }

        for (int i = 0; i < argc; i++) {
                printf("argv[%d]=%s\n", i, argv[i]);
        }

        int server_port = atoi(argv[1]);
        int max_client_connections = 64;

        /* Stores internet address information */
        struct sockaddr_in server_addr, client_addr;

        /* Zero out server's sockaddr_in struct, and populate it */
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;           // IPv4 address family
        server_addr.sin_addr.s_addr = INADDR_ANY;   // Listen on 0.0.0.0
        server_addr.sin_port = htons(server_port);  // Use network byte order

        /* Create a socket file descriptor */
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);

        /* Bind to that socket */
        bind(sockfd, (struct sockaddr *) &server_addr, sizeof(server_addr));

        /* Listen to the socket */
        listen(sockfd, max_client_connections);

        /* Extract a connection request from the queue of pending connections */
        socklen_t cli_addr_len = sizeof(client_addr);
        int client_sockfd = accept(sockfd, (struct sockaddr *) &client_addr,
                                   &cli_addr_len);

        char client_ip_str[256];
        inet_ntop(client_addr.sin_family, &client_addr.sin_addr, client_ip_str, 256);
        printf("Accepted a client connection from %s\n", client_ip_str);

        /* Allocate a static buffer and to read into */
        char buffer[MAX_MSG_SIZE] = { '\0' };

        /* Read data from the client's sockfd into the buffer */
        int n = read(client_sockfd, &buffer, MAX_MSG_SIZE);
        if (n < 0) {
                printf("Received %d from read()\n", n);
                exit(1);
        }

        /* Print buffer's contents */
        printf("Client: %s\n", buffer);

        /* Close client socket file descriptor */
        close(client_sockfd);

        /* Close listen socket file descriptor */
        close(sockfd);

        return 0;
}