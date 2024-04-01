#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

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

        int server_port = atoi(argv[2]);
        const char *server_host = argv[1];
        struct sockaddr_in server_addr;

        /* Zero out server's sockaddr_in struct, and populate it */
        memset(&server_addr, 0, sizeof(server_addr));

        /* Create a socket file descriptor */
        //int sockfd = socket(AF_INET, SOCK_STREAM, 0);

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
        while (current) {
                if (current->ai_family == AF_INET) {
                        ptr = &((struct sockaddr_in *) current->ai_addr)
                                        ->sin_addr;
                        inet_ntop(current->ai_family, ptr, addrstr, 256);
                        printf ("IPv4 address: %s (%s)\n", addrstr, 
                                current->ai_canonname);
                }
                current = current->ai_next;
        }

        /* Free our addrinfo linked-list of results */
        freeaddrinfo(result);

        return 0;
}