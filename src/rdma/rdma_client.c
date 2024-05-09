#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <netinet/in.h>	
#include <rdma/rdma_verbs.h>

/* Connection manager data structures for client */
static struct rdma_event_channel *cm_event_channel;
static struct rdma_cm_id *cm_client_id, *cm_server_id;

void print_usage() {
        printf("Usage:\n\t./rdma-client <client_host> <server_host> <server_port>\n");
        printf("Example:\n\t./rdma-client 192.168.0.106 192.168.0.105 20021\n");
}

void cleanup_client() {
        ret = rdma_destroy_id(cm_client_id);
	if (ret) {
		fprintf(stderr, "Failed to destroy client CM id with errno: (%s)\n",
                                strerror(errno));
	}
        rdma_destroy_event_channel(cm_event_channel);
}

int main(int argc, char **argv) {

        if (argc < 3) {
                print_usage();
                return 1;
        }

        const char *client_host = argv[1];
        const char *server_host = argv[2];
        int server_port = atoi(argv[3]);

        /* Open a CM event channel for asynchronous communication events */
        cm_event_channel = rdma_create_event_channel();
        if (!cm_event_channel) {
                fprintf(stderr, "Creating CM event channel failed with errno: (%s)\n",
                                strerror(errno));
		return -errno;
        }
        printf("CM event channel created at %p\n", cm_event_channel);

        /* Create CM id to track communication information */
        int ret = rdma_create_id(cm_event_channel, &cm_client_id, NULL,
                                 RDMA_PS_TCP);
        if (ret) {
                fprintf(stderr, "Creating CM id failed with errno: (%s)\n",
                                strerror(errno));
                cleanup_client();
		return -errno;
        }

        /* Set up server sockaddr_in information */
        struct sockaddr_in server_sockaddr;
        memset(&server_sockaddr, 0, sizeof(server_sockaddr));
        server_sockaddr.sin_family = AF_INET;
        server_sockaddr.sinaddr.s_addr = inet_addr(server_host);
        server_sockaddr.sin_port = htons(server_port);

        /* Optional: set up client sockaddr_in information */
        struct sockaddr_in client_sockaddr;
        memset(&client_sockaddr, 0, sizeof(client_sockaddr));
        client_sockaddr.sin_family = AF_INET;
        client_sockaddr.sinaddr.s_addr = inet_addr(client_host);

        /* Resolve destination and optional source addresses from IP addresses to
	 * an RDMA address. If successful, the specified rdma_cm_id will be bound
	 * to a local device.
         */
        int timeout_ms = 2000;
        ret = rdma_resolve_addr(cm_client_id,
                                (struct sockaddr*)&client_sockaddr,
                                (struct sockaddr*)&server_sockaddr,
                                timeout_ms);
        if (ret) {
                fprintf(stderr, "Failed to resolve RDMA addresses with errno: (%s)\n",
                                strerror(errno));
                cleanup_client();
		return -errno;
        }
        printf("Waiting to resolve RDMA addresses for client IP %s and server IP %s\n",
                client_host, server_host);

        /* Process CM event for resolving the address */
        struct rdma_cm_event *cm_event = NULL;
        ret = rdma_get_cm_event(cm_event_channel, &cm_event);
        if (ret == -1) {
                fprintf(stderr, "Blocking for CM events failed: (%s)\n",
                                strerror(errno));
                cleanup_client();
                return -errno;
        }

        /* Check status of the event */
        if (cm_event->status != 0) {
                fprintf(stderr, "CM event received with non-zero status: (%d)\n",
                        cm_event->status);
                rdma_ack_cm_event(cm_event);
                cleanup_client();
                return -1;
        } else if (cm_event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
                fprintf(stderr, "CM event received with unexpected type. Expected %s, but got %s\n",
                                rdma_event_str(RDMA_CM_EVENT_ADDR_RESOLVED),
                                rdma_event_str(cm_event->event));
                rdma_ack_cm_event(cm_event);
                cleanup_client();
                return -1;
        }

        /* We got the expected RDMA_CM_EVENT_ADDR_RESOLVED event. We need to
         * acknowledge the event.
         */
        printf("New CM event of type %s received\n",
                rdma_event_str(cm_event->event));
        ret = rdma_ack_cm_event(cm_event);
        if (ret) {
                fprintf(stderr, "Failed to ack the RDMA_CM_EVENT_ADDR_RESOLVED event: (%s)\n",
                                strerror(errno));
                cleanup_client();
                return -errno;
        }
        printf("Acknowledged RDMA_CM_EVENT_ADDR_RESOLVED event\n");

        ret = process_rdma_cm_event(cm_event_channel,
			            RDMA_CM_EVENT_ADDR_RESOLVED,
			&cm_event);


        cleanup_client();
        return 0;
}