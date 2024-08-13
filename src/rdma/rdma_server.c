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
static struct rdma_buffer_attr client_metadata;
/* Send buffer from where client will retrieve metadata about the server */
static struct rdma_buffer_attr server_metadata;
/* Server's send and receive scatter-gather entries (SGE) for work requests */
static struct ibv_sge client_recv_sge, server_send_sge;

/* Send and receive work requests, along with bad WR pointers */
static struct ibv_recv_wr client_recv_wr, *bad_client_recv_wr = NULL;
static struct ibv_send_wr server_send_wr, *bad_server_send_wr = NULL;

/* Dynamically allocated and registered memory accessible by the client */
void *server_buffer = NULL;

/* Cleans up all allocated/registered resources, in reverse order that they were
 * created, conditionally if they've been allocated or initalized.
 */
void cleanup_server()
{
        /* Destroy server memory buffer */
        if (server_buffer) {
                free(server_buffer);
        }

        /* De-register server buffer memory region */
        if (server_buffer_mr) {
                printf("Deregistering ibv_mr server_buffer_mr\n");
                ibv_dereg_mr(server_buffer_mr);
        }

        /* De-register server metadata memory region */
        if (server_metadata_mr) {
                printf("Deregistering ibv_mr server_metadata_mr\n");
                ibv_dereg_mr(server_metadata_mr);
        }

        /* De-register client metadata memory region */
        if (client_metadata_mr) {
                printf("Deregistering ibv_mr client_metadata_mr\n");
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
        print_rdma_cm_id(cm_server_id, 1);

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
        print_rdma_addrinfo(rai, 1);

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
         * This is a non-blocking call. Allow a backlog of up to 8 clients.
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
        printf("New RDMA connection stored in cm_client_id %p:\n", cm_client_id);
        print_rdma_cm_id(cm_client_id, 1);

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
        printf("Created Protection Domain for client's verbs provider:\n");
        print_ibv_pd(protection_domain, 1);

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
	completion_queue = ibv_create_cq(cm_client_id->verbs, /* which device */
		                         16, /* maximum capacity */
		                         NULL, /* user context, not used here */
		                         io_completion_channel, /* IO completion channel to use */
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
        bzero(&qp_init_attr, sizeof qp_init_attr);
        qp_init_attr.qp_type = IBV_QPT_RC; /* QP type Reliable Connection */
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
        ret = rdma_create_qp(
                cm_client_id, /* Which connection id */
                protection_domain, /* Which protection domain */
                &qp_init_attr /* Initial QP attributes */
        );
        if (ret) {
                fprintf(stderr, "Failed to create QP: %s\n",
                        strerror(errno));
		return -errno;
        }
        client_queue_pair = cm_client_id->qp;
        printf("Created QP for client on server\n");

        return ret;
}

/*
 * Pre-posts a receive buffer to capture metadata about the client:
 * 1. Register our client_metadata memory section as a memory region (MR).
 * 2. Fill out a scatter-gather entry (SGE) with info about the MR.
 * 3. Add SGE to work request (WR).
 * 4. Post WR to receive buffer of client QP.
 */
static int post_metadata_recv_buffer()
{
        /* Register memory region (MR) where client metadata will be stored */
        client_metadata_mr = ibv_reg_mr(protection_domain,
                                        &client_metadata,
                                        sizeof(client_metadata),
                                        IBV_ACCESS_LOCAL_WRITE);
        if(!client_metadata_mr){
		fprintf(stderr, "Failed to register client_metadata_mr: -ENOMEM\n");
		return -ENOMEM;
	}
        printf("Successfully registered client_metadata_mr:\n");
        print_ibv_mr(client_metadata_mr, 1);

        /* Initialize the client receive SGE with where we want the data
         * received from the client to go.
         */
        client_recv_sge.addr = (uint64_t) client_metadata_mr->addr;
        client_recv_sge.length = client_metadata_mr->length;
        client_recv_sge.lkey = client_metadata_mr->lkey;

        /* Create a WR with the client receive SGE. */
        memset(&client_recv_wr, 0, sizeof(client_recv_wr));
        client_recv_wr.sg_list = &client_recv_sge;
        client_recv_wr.num_sge = 1;
        /* Pre-post the WR to the client queue-pair */
        int ret = ibv_post_recv(client_queue_pair, /* client QP */
                                &client_recv_wr, /* Recieve WR */
                                &bad_client_recv_wr /* Error WR */
                               );
        if (ret) {
                fprintf(stderr, "Failed to pre-post client receive WR to QP: %s\n",
                        strerror(ret));
        }
        printf("Successfully pre-posted client metadata receive buffer to client QP:\n");
        print_ibv_mr(client_metadata_mr, 1);

        return ret;
}

/*
 * Accept a client connection:
 * 1. Fill out connection parameters for the connection we're accepting.
 * 2. Accept the client connection using rdma_accept().
 * 3. Wait for RDMA_CM_EVENT_ESTABLISHED event, ACKing when received.
 */
static int accept_client_connection()
{
        struct rdma_conn_param conn_param;
	struct rdma_cm_event *cm_event = NULL;

        /* Before we accept a connection we have to fill out an rdma_conn_param
         * struct containing connection properties:
         *
         * - initiator_depth: The maximum number of outstanding RDMA read and
         * atomic operations that the local side will have to the remote
         * side.
         * - responder_resources: The maximum number of outstanding RDMA read
         * and atomic operations that the local side will accept from the remote
         * side.
         * - retry_count: The maximum number of times that a data transfer
         * operation should be retried on the connection when an error occurs.
         */
        conn_param.initiator_depth = 3;
	conn_param.responder_resources = 3;
	conn_param.retry_count = 3;
        /* Note how we use rdma_accept() here instead of the client's
         * rdma_connect(). After this, we'll expect an RDMA_CM_EVENT_ESTABLISHED
         * CM event.
         */
        int ret = rdma_accept(cm_client_id, &conn_param);
        if (ret) {
                fprintf(stderr, "Failed to accept connection from client: %s\n",
                        strerror(errno));
		return -errno;
        }
        ret = process_rdma_event(cm_event_channel, &cm_event,
                                 RDMA_CM_EVENT_ESTABLISHED);
        if (ret) {
                fprintf(stderr, "Failed to process CM event\n");
                return ret;
        }

        /* We got the expected RDMA_CM_EVENT_ESTABLISHED event. ACK the event
         * to free the allocated memory.
         */
        printf("New CM event of type %s received\n",
                rdma_event_str(cm_event->event));
        ret = rdma_ack_cm_event(cm_event);
        if (ret == -1) {
                fprintf(stderr, "Failed to ACK CM event %s: (%s)\n",
                                rdma_event_str(cm_event->event),
                                strerror(errno));
		return -errno;
        }
        printf("Successfully accepted connection from client RDMA device\n");

        /* Optional: extract connection information from cm_client_id */
        struct sockaddr_in client_sockaddr = { 0 };
        memcpy(&client_sockaddr, rdma_get_peer_addr(cm_client_id),
               sizeof(client_sockaddr));
        printf("Client connection accepted from %s\n",
               inet_ntoa(client_sockaddr.sin_addr));
        return 0;
}

/*
 * Exchange metadata with the client via pre-registered buffers.
 *
 * Manpages: https://man7.org/linux/man-pages/man3/ibv_reg_mr.3.html
 *           https://man7.org/linux/man-pages/man3/ibv_post_send.3.html
 * RDMAmojo: https://www.rdmamojo.com/2012/09/07/ibv_reg_mr/
 *           https://www.rdmamojo.com/2013/01/26/ibv_post_send/
 */
static int exchange_metadata_with_client()
{
        /* We start off by receiving the metadata about the client into the
         * pre-posted receive buffer client_metadata (we posted this in
         * post_metadata_recv_buffer()).
         */
        int ret = 0;
        int expected_wc = 1;
        struct ibv_wc work_completions[expected_wc];

        /* Wait for client to send its metadata info. We will receive a work
         * completion (WC) notification for our pre-posted receive request.
         */
        ret = process_work_completion_event(
                io_completion_channel,
                work_completions,
                expected_wc
        );
        if (ret != expected_wc) {
                fprintf(stderr, "Failed to process %d Work Completions: ret=%d\n",
                        expected_wc, ret);
		return ret;
        }
        printf("Got %d Work Completions\n", ret);
        printf("Now have client_metadata: ");
        print_rdma_buffer_attr(&client_metadata, 1);

        /* Next, we need to satisfy the client's request for the server's
         * metadata.
         */

        /* Allocate and register the memory region where the client will
         * read/write the message from/to.
         */
        server_buffer_mr = create_rdma_buffer(
                protection_domain,
                client_metadata.length,  /* Size of the source message from the client */
                (IBV_ACCESS_LOCAL_WRITE|
                 IBV_ACCESS_REMOTE_READ|
                 IBV_ACCESS_REMOTE_WRITE) /* Access permissions */
        );
        if (!server_buffer_mr) {
                fprintf(stderr, "Failed to allocate/register server_buffer_mr\n");
		return -1;
        }

        /* Set our server_buffer address from MR so we can free it later */
        server_buffer = server_buffer_mr->addr;

        /* We need to now send metadata about the above buffer to the client.
         * This will complete the client's posted WR for server metadata.
         */

        /* Prepare the server metadata buffer with information about the MR
         * we just registered above.
         */
        server_metadata.address = (uint64_t) server_buffer_mr->addr;
        server_metadata.length = server_buffer_mr->length;
        server_metadata.stag.local_stag = server_buffer_mr->lkey;

        /* Register server metadata MR */
        server_metadata_mr = ibv_reg_mr(
                protection_domain, /* Server's PD */
		&server_metadata, /* Server's metadata buffer */
		sizeof(server_metadata), /* Size of server's metadata buffer */
		IBV_ACCESS_LOCAL_WRITE /* Only allow our RDMA device to write */
        );
        if (!server_metadata_mr) {
		fprintf(stderr, "Failed to register server_metadata_mr: %s\n",
                        strerror(errno));
		return -errno;
	}
        printf("Registered server_metadata_mr:");
        print_ibv_mr(server_metadata_mr, 1);

        /* Populate the server send SGE with information about our metadata MR
         */
	server_send_sge.addr = (uint64_t) client_metadata_mr->addr;
	server_send_sge.length = (uint32_t) client_metadata_mr->length;
	server_send_sge.lkey = client_metadata_mr->lkey;

        /* Link to the send WR. This is a SEND operation, meaning it will
         * complete some RECV WR.
         */
        bzero(&server_send_wr, sizeof(server_send_wr));
	server_send_wr.sg_list = &server_send_sge;
	server_send_wr.num_sge = 1;
	server_send_wr.opcode = IBV_WR_SEND;
	server_send_wr.send_flags = IBV_SEND_SIGNALED;

        /* Post the send WR to the client QP, containing metadata information
         * that the client requested.
         */
        ret = ibv_post_send(
                client_queue_pair,
                &server_send_wr,
                &bad_server_send_wr
        );
        if (ret) {
                fprintf(stderr, "Failed to send server metadata: %s\n",
                        strerror(errno));
		return -errno;
        }
        printf("Sent server metadata to client\n");

        /* Process WC event for satisfying the client's WR. We can reuse the
         * same work_completions array and expected_wc count from above.
         */
        ret = process_work_completion_event(
                io_completion_channel,
                work_completions,
                expected_wc
        );
        if (ret != expected_wc) {
                fprintf(stderr, "Failed to process %d Work Completions: ret=%d\n",
                        expected_wc, ret);
		return ret;
        }
        printf("Got %d Work Completions\n", ret);
        return 0;
}

/*
 * disconnect_from_client waits for an RDMA_CM_EVENT_DISCONNECTED CM event
 * from the client, indicating the client has disconnected.
 */
static int disconnect_from_client()
{
        struct rdma_cm_event *cm_event = NULL;
        int ret = 0;
        ret = process_rdma_event(cm_event_channel, &cm_event,
                                        RDMA_CM_EVENT_DISCONNECTED);
        if (ret) {
                fprintf(stderr, "Failed to process CM event\n");
                return ret;
        } else {
                printf("Received CM event of type %s\n",
                        rdma_event_str(cm_event->event));
        }
        ret = rdma_ack_cm_event(cm_event);
        if (ret) {
                fprintf(stderr, "Failed to ACK CM event\n");
                ret = 0;
        }

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
        if (ret) {
                cleanup_server();
                return ret;
        }

        ret = setup_communication_resources();
        if (ret) {
                cleanup_server();
                return ret;
        }

        ret = post_metadata_recv_buffer();
        if (ret) {
                cleanup_server();
                return ret;
        }

        ret = accept_client_connection();
        if (ret) {
                cleanup_server();
                return ret;
        }

        ret = exchange_metadata_with_client();
        if (ret) {
                cleanup_server();
                return ret;
        }

        ret = disconnect_from_client();
        if (ret) {
                cleanup_server();
                return ret;
        }

        /* Clean up all dynamically allocated resources */
        cleanup_server();
        return ret;
}