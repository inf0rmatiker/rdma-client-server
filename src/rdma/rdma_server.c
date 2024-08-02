/*
 * Description:
 *      RDMA server implementation
 * Author:
 *      Caleb Carlson <ccarlson355@gmail.com>
 * References:
 *      https://github.com/linux-rdma/rdma-core/blob/master/librdmacm/examples
 *      https://github.com/animeshtrivedi/rdma-example
 */

#include "rdma_common.h"

static char *server_addr = "127.0.0.1";
static char *server_port = "7471";

/* Connection Manager data structures for server */
static struct rdma_event_channel *cm_event_channel;
static struct rdma_cm_id *cm_server_id, *cm_client_id;
static struct rdma_addrinfo *rai, hints;

/* RDMA Queue Pair and Protection Domain resources */
static struct ibv_pd *protection_domain = NULL;
static struct ibv_qp *client_queue_pair = NULL;
static struct ibv_cq *completion_queue = NULL;
static struct ibv_comp_channel *io_completion_channel = NULL;
static struct ibv_qp_init_attr qp_init_attr;

/* Memory resources */
static struct ibv_mr *client_metadata_mr = NULL;
static struct ibv_mr *server_metadata_mr = NULL;
static struct ibv_mr *server_buffer_mr = NULL;

/* Receive buffer to which the server will store metadata about the client */
static struct rdma_buffer_attr client_metadata_attr;
/* Send buffer from where client will retrieve metadata about the server */
static struct rdma_buffer_attr server_metadata_attr;
/* Server's send and receive scatter-gather entries (SGE) for work requests */
static struct ibv_sge client_recv_sge, server_send_sge;

/* Send and receive work requests, along with bad WR pointers */
static struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
static struct ibv_send_wr server_send_wr, *bad_server_send_wr = NULL;

/* Dynamically allocated and registered memory accessible by the client */
char *server_buffer = NULL;

/* Cleans up all allocated/registered resources, in reverse order that they were
 * created, conditionally if they've been allocated or initalized.
 */
void cleanup_server()
{
        /* Destroy server memory buffer */
        if (server_buffer) {
                free(server_buffer);
        }

        /* Destroy client metadata buffer */
        if (client_metadata_mr) {
                ibv_dereg_mr(client_metadata_mr);
        }

        /* Destroy queue pairs */
        if (client_queue_pair) {
                printf("Destroying queue pairs\n");
                ibv_destroy_qp(client_queue_pair);
        }

        /* Destroy completion queue */
        if (completion_queue) {
                printf("Destroying completion queue\n");
                ibv_destroy_cq(completion_queue);
        }

        /* Destroy completion channel */
        if (io_completion_channel) {
                printf("Destroying I/O completion channel\n");
                ibv_destroy_comp_channel(io_completion_channel);
        }

        /* Deallocate protection domain*/
        if (protection_domain) {
                printf("Deallocating protection domain\n");
                ibv_dealloc_pd(protection_domain);
        }

        /* Destroy client/server CM connection ids */
        if (cm_client_id) {
                printf("Freeing client CM id\n");
                rdma_destroy_id(cm_client_id);
        }
        if (cm_server_id) {
                printf("Freeing server CM id\n");
                rdma_destroy_id(cm_server_id);
        }

        /* Clean up server RDMA addrinfo struct */
        if (rai) {
                printf("Freeing server rdma_addrinfo\n");
                rdma_freeaddrinfo(rai);
        }

        /* Clean-up and destroy CM event channel */
        if (cm_event_channel) {
                printf("Destroying server CM event channel\n");
                rdma_destroy_event_channel(cm_event_channel);
        }
	printf("Successfully cleaned up all server resources.\n");
}

/*
 * Sets up the initial connection resources for the server:
 * 1. Create Connection Manager event channel
 * 2. Create Connection Manager id for the server
 * 3. Get RDMA address info for our RDMA device
 * 4. Bind our RDMA device to an address
 * 5. Set up the server to listen on that address
 * 6. Block for an RDMA_CM_EVENT_CONNECT_REQUEST event on
 *    the event channel, capturing the client's CM id when
 *    we receive one.
 * 7. Ack the event, freeing it as a result.
 */
int setup_server()
{
        int ret = 0;

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
        ret = rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP);
        if (ret == -1) {
                fprintf(stderr, "Creating CM id failed with errno: (%s)\n",
                                strerror(errno));
		return -errno;
        }
        printf("Server CM id is created\n");
        print_rdma_cm_id(cm_server_id, 0);

        /* Figure out the rdma_addrinfo of our RDMA device. */
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = RAI_NUMERICHOST | RAI_PASSIVE;
        hints.ai_port_space = RDMA_PS_TCP;
        ret = rdma_getaddrinfo(server_addr, server_port, &hints, &rai);
        if (ret) {
                fprintf(stderr, "Failed rdma_getaddrinfo with errno: (%s)\n",
                                strerror(errno));
                return -errno;
        }
        printf("Successfully retrieved rdma_addrinfo\n");
        print_rdma_addrinfo(rai, 0);

        /* Bind to an RDMA address. */
        ret = rdma_bind_addr(cm_server_id, rai->ai_src_addr);
	if (ret) {
		fprintf(stderr, "Failed rdma_bind_addr with errno: (%s)\n",
                        strerror(errno));
                return -errno;
	}
        printf("Successfully bound RDMA server address %s:%s\n", server_addr,
               server_port);

        /* Initiate a listen on the RDMA IP address and port.
         * This is a non-blocking call.
         */
        ret = rdma_listen(cm_server_id, 8);
        if (ret == -1) {
                fprintf(stderr, "Listening for CM events failed: (%s)\n",
                                strerror(errno));
		return -errno;
        }
        printf("Server is listening successfully at: %s, port: %d \n",
               inet_ntoa(((struct sockaddr_in *)rai->ai_src_addr)->sin_addr),
	       ntohs(((struct sockaddr_in *)rai->ai_src_addr)->sin_port));

        /* We expect the client to connect and generate a
         * RDMA_CM_EVENT_CONNECT_REQUEST. We wait (block) on the
         * CM event channel for this event.
	 */
        struct rdma_cm_event *cm_event = NULL;
        ret = process_rdma_event(cm_event_channel, &cm_event,
                                 RDMA_CM_EVENT_CONNECT_REQUEST);
        if (ret) {
                fprintf(stderr, "Failed to process CM event\n");
                return ret;
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
		return -errno;
        }
        printf("New RDMA connection stored at %p\n", cm_client_id);

        return ret;
}

/*
 * Establish IB Verbs communication resources, allowing
 * us to communicate with client RDMA device:
 * 1. Set up Protection Domain, using client's RDMA device verbs provider
 * 2. Set up I/O completion channel, using client's RDMA device verbs provider
 * 3. Set up a Completion Queue for Work Completion metadata
 * 4. Request notifications for all event types on CQ
 * 5. Create a Queue Pair using initial attributes
 */
int setup_communication_resources()
{
        int ret = 0;

        /* Create a Protection Domain (PD) using the CM id we got from the
         * client connection request earlier. That cm_id->verbs defines the
         * verbs provider of the client's RDMA device.
         */
        protection_domain = ibv_alloc_pd(cm_client_id->verbs);
        if (!protection_domain) {
                fprintf(stderr, "Failed to create Protection Domain: %s\n",
                        strerror(errno));
                return -errno;
        }
        printf("Created Protection Domain\n");

        /* Create a Completion Channel (CC) where I/O completion notifications
         * are sent. A CC is tied to an RDMA device, so we will use
         * cm_client_id->verbs here.
         */
        io_completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
        if (!io_completion_channel) {
                fprintf(stderr, "Failed to create Completion Channel: %s\n",
                        strerror(errno));
                return -errno;
        }
        printf("Created I/O Completion Channel\n");

        /* Create a Completion Queue (CQ) where actual I/O completion
	 * metadata is placed. The metadata is packed into a
	 * struct ibv_wc (wc = work completion). ibv_wc has detailed
	 * information about the work completion. An I/O request in RDMA world
	 * is called "work"
	 */
	completion_queue = ibv_create_cq(
                cm_client_id->verbs /* which device */,
		16 /* maximum capacity */,
		NULL /* user context, not used here */,
		io_completion_channel /* IO completion channel to use */,
		0 /* signaling vector, not used here */
        );
	if (!completion_queue) {
                fprintf(stderr, "Failed to create Completion Queue: %s\n",
                        strerror(errno));
		return -errno;
	}
	printf("Completion Queue (CQ) is created at with %d elements\n",
	       completion_queue->cqe);

        /* Ask CQ to give us all events, and not filter any */
        ret = ibv_req_notify_cq(
                completion_queue, /* which CQ we're requesting notifications */
                0 /* request all event types, no filter*/
        );
        if (ret) {
                fprintf(stderr, "Failed to request notifications for all event types on CQ: %s\n",
                        strerror(errno));
		return -errno;
        }

        /* Set up the Queue Pairs (send, receive) and their capacity.
         * The capacity here is defined statically, but we could dynamically
         * probe these values from the device if we wanted to.
         *
         * To do this, we first need to set up the QP initial requested
         * attributes struct (ibv_qp_init_attr).
         */
        memset(&qp_init_attr, 0, sizeof qp_init_attr);
        qp_init_attr.cap.max_recv_sge = 2; /* Max SGE per receive posting */
        qp_init_attr.cap.max_send_sge = 2; /* Max SGE per send posting */
        qp_init_attr.cap.max_recv_wr = 8;  /* Max receive posting capacity */
        qp_init_attr.cap.max_send_wr = 8;  /* Max send posting capacity */
        /* Use the same CQ for both send/receive completion events */
        qp_init_attr.recv_cq = completion_queue; /* Where to notify for receive completion operations */
        qp_init_attr.send_cq = completion_queue; /* Where to notify for send completion operations */

        /* Finally, create a QP. After this call, the ibv_qp reference will be
         * stored in the client's CM id: client_cm_id->qp.
         */
        printf("cm_client_id: \n");
        print_rdma_cm_id(cm_client_id, 0);
        ret = rdma_create_qp(
                cm_client_id, /* Which connection id */
                protection_domain, /* Which protection domain */
                &qp_init_attr /* Initial QP attributes */
        );
        if (ret) {
                fprintf(stderr, "DEBUG\n");
                if (cm_client_id->qp) {
                        printf("cm_client_id->qp is NOT NULL!\n");
                }
                printf("cm_client_id->verbs: %p\n", cm_client_id->verbs);
                printf("qp_init_attr.qp_context: %p\n", qp_init_attr.qp_context);
                fprintf(stderr, "Failed to create QP: %s\n",
                        strerror(errno));
		return -errno;
        }
        client_queue_pair = cm_client_id->qp;
        printf("Created QP for client on server\n");

        return ret;
}

void print_usage()
{
        printf("Usage\n");
        printf("\t./rdma-server -s <server_address> -p <server_port>\n");
        printf("Example\n");
        printf("\t./rdma-server -s 192.168.0.106 -p 7471\n");
}

int main(int argc, char **argv)
{
        int option;
        while ((option = getopt(argc, argv, "s:p:")) != -1) {
                switch (option) {
                        case 's':
                                server_addr = optarg;
                                break;
                        case 'p':
                                server_port = optarg;
                                break;
                        default:
                                print_usage;
                                exit(1);
                }

        }

        int ret = setup_server();
        if (ret)
                fprintf(stderr, "Failed to establish the server\n");
        ret = setup_communication_resources();
        if (ret)
                fprintf(stderr, "Failed to establish communication resources\n");

        /* Allocate server buffer */
        server_buffer = (char *)calloc(4096, sizeof(char));

        /* Clean up all dynamically allocated resources */
        cleanup_server();
        return ret;
}