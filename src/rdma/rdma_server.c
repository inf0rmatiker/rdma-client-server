#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

/* Connection manager data structures for server */
static struct rdma_event_channel *cm_event_channel;
static struct rdma_cm_id *cm_server_id, *cm_client_id;

/* Cleans up Connection Manager ID and Event Channel objects before exiting.
 */
void cleanup_server() {
        /* Destroy server connection identifier */
        int ret = rdma_destroy_id(cm_server_id);
	if (ret == -1) {
		fprintf(stderr, "Failed to destroy server CM id: (%s)\n",
                        strerror(errno));
	}

        /* Clean-up and destroy CM event channel */
        rdma_destroy_event_channel(cm_event_channel);
	printf("Successfully destroyed CM event channel\n");
}

int main(int argc, char **argv) {

        /* Create CM event channel for asynchronous communication events */
        cm_event_channel = rdma_create_event_channel();
	if (!cm_event_channel) {
                fprintf(stderr, "Creating CM event channel failed with errno: (%s)\n",
                                strerror(errno));
		return -errno;
	}
        printf("RDMA CM event channel is created successfully at %p\n",
	       cm_event_channel);

        /* Create connection identifier for the RDMA connection */
        struct rdma_cm_id *cm_server_id = NULL;
        int ret = rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP);
        if (ret == -1) {
                fprintf(stderr, "Creating CM id failed with errno: (%s)\n",
                                strerror(errno));
		return -errno;
        }
        printf("Server CM id is created\n");

        /* Bind to an RDMA address. To do this, we need to create a sockaddr_in,
         * fill out the fields, then cast it to a sockaddr* before passing it to
         * rdma_bind_addr.
         */
        struct sockaddr_in server_sockaddr;
        memset(&server_sockaddr, 0, sizeof(server_sockaddr));
	server_sockaddr.sin_family = AF_INET; /* standard IP NET address */
	server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY); /* passed address */
        server_sockaddr.sin_port = htons(20021);

        ret = rdma_bind_addr(cm_server_id, (struct sockaddr *)&server_sockaddr);
        if (ret == -1) {
                fprintf(stderr, "Binding server RDMA address failed with errno: (%s)\n",
                                strerror(errno));
                cleanup_server();
		return -errno;
        }
        printf("Successfully bound RDMA server address 0.0.0.0:20021\n");

        /* Initiate a listen on the RDMA IP address and port.
         * This is a non-blocking call.
         */
        ret = rdma_listen(cm_server_id, 8);
        if (ret == -1) {
                fprintf(stderr, "Listening for CM events failed: (%s)\n",
                                strerror(errno));
                cleanup_server();
		return -errno;
        }
        printf("Server is listening successfully at: %s, port: %d \n",
               inet_ntoa(server_sockaddr.sin_addr),
	       ntohs(server_sockaddr.sin_port));

        /* We expect the client to connect and generate a
         * RDMA_CM_EVENT_CONNECT_REQUEST. We wait (block) on the
         * connection management event channel for this event.
	 */
        struct rdma_cm_event *cm_event = NULL;
	ret = rdma_get_cm_event(cm_event_channel, &cm_event);
        if (ret == -1) {
                fprintf(stderr, "Blocking for CM events failed: (%s)\n",
                                strerror(errno));
                cleanup_server();
                return -errno;
        }

        /* At this point, we've received a CM event. We need to check its
         * status and event type.
         */
        if (cm_event->status != 0) {
                fprintf(stderr, "CM event received with non-zero status: (%d)\n",
                                cm_event->status);

                /* Even if we get a bad status we still need to ACK the event */
                rdma_ack_cm_event(cm_event);
                cleanup_server();
                return -1;
        } else if (cm_event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
                fprintf(stderr, "CM event received with unexpected type. Expected %s, but got %s\n",
                                rdma_event_str(RDMA_CM_EVENT_CONNECT_REQUEST),
                                rdma_event_str(cm_event->event));
                /* Even if we got unexpected event type we still need to ACK
                 * the event.
                 */
                rdma_ack_cm_event(cm_event);
                cleanup_server();
                return -1;
        }

        /* We got the expected RDMA_CM_EVENT_CONNECT_REQUEST event */
        printf("New CM event of type %s received\n",
                rdma_event_str(cm_event->event));

        /* Much like TCP connection, listening returns a new connection
         * id for a newly connected client. In the case of RDMA, this is stored
         * in the cm_event->id field. We need to save this information before
         * acknowledging the event, which also frees the struct.
	 */
	cm_client_id = cm_event->id;
        ret = rdma_ack_cm_event(cm_event);
        if (ret == -1) {
                fprintf(stderr, "Failed to ACK CM event %s: (%s)\n",
                                rdma_event_str(cm_event->event),
                                strerror(errno));
                rdma_destroy_id(cm_client_id);
                cleanup_server();
		return -errno;
        }
        printf("New RDMA connection stored at %p\n", cm_client_id);


        /* Cleanup before exiting */
        rdma_destroy_id(cm_client_id);
        cleanup_server();
        return 0;
}