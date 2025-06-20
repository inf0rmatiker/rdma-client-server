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

void send_message() {

}

void read_stdin(char *buf, int max_size) {
        if (!buf) {
                return;
        }

        char *str_read = fgets(buf, max_size, stdin);
        if (!str_read) {
                fprintf(stderr, "Unable to read from stdin: %s\n",
                        strerror(errno));
                return;
        }
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
        printf("Created client sockfd: %d\n", client_sockfd);

        /* Connect to server socket */
        int res = connect(client_sockfd, (struct sockaddr*)&server_addr,
                          sizeof(server_addr));
        if (res < 0) {
                printf("Connection failed: %s\n", strerror(errno));
                return -1;
        }
        printf("Connected to server: %s:%d\n", server_host, server_port);

        char *send_buffer = calloc(sizeof(char), MAX_MSG_SIZE+1);
        read_stdin(send_buffer, MAX_MSG_SIZE);

        int bytes_sent = send(client_sockfd, send_buffer, strlen(send_buffer), 0);
        printf("Sent %d bytes\n", bytes_sent);

        /* Cleanup our client sockfd */
        close(client_sockfd);

        /* Free our send buffer */
        free(send_buffer);
        send_buffer = NULL;

        return 0;
}