#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <stdbool.h>
#include "socket_common.h"

void print_usage() {
        printf("Usage:\n\t./socket-client <server_host> <server_port>\n");
        printf("Example:\n\t./socket-client 10.214.131.9 8082\n");
}

int main(int argc, char **argv) {

        if (argc < 2) {
                print_usage();
                return 1;
        }

        for (int i = 0; i < argc; i++) {
                printf("argv[%d]=%s\n", i, argv[i]);
        }

        const char *server_host = argv[1];
        int server_port = atoi(argv[2]);
        if (!is_valid_port(server_port)) {
                fprintf(stderr, "'%d' is an invalid server port choice\n",
                        server_port);
        }

        /* Set up server sockaddr_in struct */
        struct sockaddr_in server_addr;
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = inet_addr(server_host);
        server_addr.sin_port = htons(server_port);

        /* Create a client socket file descriptor */
        int client_sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (client_sockfd == -1) {
                fprintf(stderr, "Unable to create client socket: %s\n",
                        strerror(errno));
                exit(1);
        }

        /* Connect to server socket */
        int res = 0;
        res = connect(client_sockfd, (struct sockaddr*)&server_addr,
                      sizeof(server_addr));
        if (res < 0) {
                printf("Connection failed: %s\n", strerror(errno));
                return -1;
        }

        char* hello = "Hello from client";

        int bytes_sent = send(client_sockfd, hello, strlen(hello), 0);
        printf("Sent %d bytes\n", bytes_sent);


        /* Cleanup our client sockfd */
        close(client_sockfd);

        return 0;
}