#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#include "socket_common.h"

void print_usage() {
        printf("Usage:\n\t./socket-server <listen_port>\n");
        printf("Example:\n\t./socket-server 8082\n");
}

/* accept_connection() listens on the server socket for incoming client
 * connections, accepts one, then returns the resulting socket file
 * descriptor.
 */
int accept_connection(int server_sockfd) {
        struct sockaddr_in client_addr;

        /* Extract a connection request from the queue of pending connections */
        socklen_t cli_addr_len = sizeof(client_addr);
        int client_sockfd = accept(server_sockfd, (struct sockaddr *) &client_addr,
                                   &cli_addr_len);

        char client_ip_str[256];
        inet_ntop(client_addr.sin_family, &client_addr.sin_addr, client_ip_str,
                  256);
        printf("Accepted a client connection from %s\n", client_ip_str);
        return client_sockfd;
}

/* read_client() loops on the blocking call read(), printing any messages
 * received, and notifies the user when a client has disconnected.
 */
void read_client(int client_sockfd) {
        /* Allocate a static buffer and to read into */
        char buffer[MAX_MSG_SIZE+1] = { '\0' };

        while (true) {
                /* Read data from the client's sockfd into the buffer */
                int n = read(client_sockfd, &buffer, MAX_MSG_SIZE);
                if (n == 0) {
                        printf("Client has disconnected.\n");
                        break;
                }
                if (n == -1) {
                        fprintf(stderr, "Error reading message: %s\n",
                                strerror(errno));
                }

                /* Print buffer's contents */
                printf("Client: %s\n", buffer);
        }
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
        if (!is_valid_port(server_port)) {
                fprintf(stderr, "'%d' is an invalid server port choice\n",
                        server_port);
                exit(1);
        }
        int max_client_connections = 64;

        /* Stores internet address information */
        struct sockaddr_in server_addr, client_addr;

        /* Zero out server's sockaddr_in struct, and populate it */
        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;           // IPv4 address family
        server_addr.sin_addr.s_addr = inet_addr("192.168.0.106");   // Listen on 0.0.0.0
        server_addr.sin_port = htons(server_port);  // Use network byte order

        /* Create a socket file descriptor */
        int sockfd = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd == -1) {
                fprintf(stderr, "Unable to create server socket: %s\n",
                        strerror(errno));
                exit(1);
        }

        /* Bind to that socket */
        int ret = bind(sockfd, (struct sockaddr *) &server_addr,
                       sizeof(server_addr));
        if (ret) {
                fprintf(stderr, "Unable to bind to socket: %s\n",
                        strerror(errno));
                exit(1);
        }
        printf("Successfully bound to sockfd: %d\n", sockfd);

        /* Listen to the socket */
        ret = listen(sockfd, max_client_connections);
        if (ret) {
                fprintf(stderr, "Unable to listen to socket: %s\n",
                        strerror(errno));
                exit(1);
        }
        printf("Listening to sockfd %d...\n", sockfd);

        while (true) {
                /* Accept a client connection */
                int client_sockfd = accept_connection(sockfd);

                read_client(client_sockfd);

                /* Close client socket file descriptor */
                close(client_sockfd);
        }

        /* Close listen socket file descriptor */
        close(sockfd);

        return 0;
}