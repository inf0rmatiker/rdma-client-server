#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>

void print_usage() {
        printf("Usage:\n\t./client <server_host> <server_port>\n");
        printf("Example:\n\t./client 10.214.131.9 8082\n");
}

int main(int argc, char** argv) {

        if (argc < 2) {
                print_usage();
                return 1;
        }

        for (int i = 0; i < argc; i++) {
                printf("argv[%d]=%s\n", i, argv[i]);
        }

        const char *server_host = argv[1];
        int server_port = atoi(argv[2]);

        /* Restricts the returned addresses to IPv4 TCP sockets */
        struct addrinfo hints;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        struct addrinfo *current, *result;
        void *ptr;  // Stores the cast sockaddr_in->sin_addr field of a result

        /* Get a linked-list of server addrinfo objects based on hints' criteria
         * and server host name.
         */
        getaddrinfo(server_host, NULL, &hints, &result);

        /* Iterate over linked-list and check information */
        current = result;
        char addrstr[256];
        int result_count = 0;
        while (current) {
                result_count++;
                if (current->ai_family == AF_INET) {
                        ptr = &((struct sockaddr_in *) current->ai_addr)
                                        ->sin_addr;
                        inet_ntop(current->ai_family, ptr, addrstr, 256);
                        printf ("IPv4 address: %s\n", addrstr);
                }
                current = current->ai_next;
        }

        inet_ntop(result->ai_family, ptr, addrstr, 256);
        printf("Found a total of %d results for %s. Using %s.\n",
               result_count, server_host, addrstr);

        /* Fill out server sockaddr_in struct from result */
        struct sockaddr_in server_addr = { 0 };
        struct sockaddr_in *res_in = (struct sockaddr_in *)result->ai_addr;
        server_addr.sin_family = res_in->sin_family;
        server_addr.sin_addr = res_in->sin_addr;
        server_addr.sin_len = res_in->sin_len;
        server_addr.sin_port = htons(server_port);

        /* Create a socket file descriptor */
        int client_sockfd = socket(AF_INET, SOCK_STREAM, 0);

        /* Connect to server socket */
        int res = 0;
        res = connect(client_sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (res < 0) {
                printf("Connection failed\n");
                return -1;
        }

        char* hello = "Hello from client";

        int bytes_sent = send(client_sockfd, hello, strlen(hello), 0);
        printf("Sent %d bytes\n", bytes_sent);


        /* Cleanup our client sockfd */
        close(client_sockfd);

        /* Free our addrinfo linked-list of results */
        freeaddrinfo(result);

        return 0;
}