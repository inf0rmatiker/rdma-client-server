/*
 * Description:
 *      RDMA client implementation
 * Author:
 *      Caleb Carlson <ccarlson355@gmail.com>
 * References:
 *      https://github.com/linux-rdma/rdma-core/blob/master/librdmacm/examples
 *      https://github.com/animeshtrivedi/rdma-example
 */

#include "rdma_common.h"

/* Server default ipoib information */
static char *server_addr = "127.0.0.1";
static char *server_port = "7471";

/* Message source buffer from where we'll write to the server */
static char *src_buffer = NULL;

/* Where the message will end up after we read it back from the server */
static char *dst_buffer = NULL;

/* --- Connection Manager data structures for client --- */
static struct rdma_event_channel *cm_event_channel;
static struct rdma_cm_id *cm_client_id, *cm_server_id;
static struct rdma_addrinfo *rai, hints;

/* --- RDMA Queue Pair and Protection Domain resources --- */
static struct ibv_pd *protection_domain = NULL;
static struct ibv_comp_channel *completion_channel = NULL;
static struct ibv_cq *completion_queue = NULL;
static struct ibv_qp_init_attr qp_init_attr;
static struct ibv_qp *queue_pair = NULL;

/* --- Scatter-Gather Entry resources */
static struct ibv_sge client_send_sge, server_recv_sge;

/* --- Work Request resources */
static struct ibv_send_wr client_send_wr, *bad_client_send_wr;
static struct ibv_recv_wr server_recv_wr, *bad_server_recv_wr;

/* --- Memory resources --- */
/* Packed static structs where we'll store buffer metadata for the client and
 * server. Things like the remote key or local key, length of buffer, and
 * address are packed into these structs.
 */
static struct rdma_buffer_attr client_metadata, server_metadata;

/* IBVerbs registered memory regions */
static struct ibv_mr *client_metadata_mr = NULL;
static struct ibv_mr *server_metadata_mr = NULL;
static struct ibv_mr *client_src_mr = NULL;
static struct ibv_mr *client_dst_mr = NULL;

static void cleanup_client()
{
        int ret = 0;

        if (cm_client_id) {
                ret = rdma_disconnect(cm_client_id);
                if (ret) {
                        fprintf(stderr, "Disconnecting from server failed with errno: (%s)\n",
                                strerror(errno));
                        ret = 0;
                } else {
                        printf("Successfully disconnected from server\n");
                }
                struct rdma_cm_event *cm_event = NULL;
                ret = process_rdma_event(cm_event_channel, &cm_event,
                                         RDMA_CM_EVENT_DISCONNECTED);
                if (ret) {
                        fprintf(stderr, "Failed to process CM event\n");
                        ret = 0;
                } else {
                        printf("Received CM event of type %s\n",
                               rdma_event_str(cm_event->event));
                }
                ret = rdma_ack_cm_event(cm_event);
                if (ret) {
                        fprintf(stderr, "Failed to ACK CM event\n");
                        ret = 0;
                }
        }

        if (src_buffer) {
                printf("Freeing message buffer\n");
                free(src_buffer);
        }

        if (dst_buffer) {
                printf("Freeing dst_buffer\n");
                free(dst_buffer);
        }

        if (client_metadata_mr) {
                printf("Deregistering ibv_mr client_metadata_mr\n");
                ibv_dereg_mr(client_metadata_mr);
        }

        if (client_src_mr) {
                printf("Deregistering ibv_mr client_src_mr\n");
                ibv_dereg_mr(client_src_mr);
        }

        if (client_dst_mr) {
                printf("Deregistering ibv_mr client_dst_mr\n");
                ibv_dereg_mr(client_dst_mr);
        }

        if (server_metadata_mr) {
                printf("Deregistering ibv_mr server_metadata_mr\n");
                ibv_dereg_mr(server_metadata_mr);
        }

        if (queue_pair) {
                printf("Destroying ibv_qp queue_pair\n");
                ibv_destroy_qp(queue_pair);
        }

        if (completion_queue) {
                printf("Destroying ibv_cq completion_queue\n");
                ibv_destroy_cq(completion_queue);
        }

        if (completion_channel) {
                printf("Destroying ibv_comp_channel completion_channel\n");
                ibv_destroy_comp_channel(completion_channel);
        }

        if (protection_domain) {
                printf("Deallocating ibv_pd protection_domain\n");
                ibv_dealloc_pd(protection_domain);
        }

        if (rai) {
                printf("Freeing rdma_addrinfo rai\n");
                rdma_freeaddrinfo(rai);
        }

        if (cm_client_id) {
                printf("Destroying rdma_cm_id cm_client_id\n");
                rdma_destroy_id(cm_client_id);
        }

        if (cm_server_id) {
                printf("Destroying rdma_cm_id cm_server_id\n");
                rdma_destroy_id(cm_server_id);
        }

        if (cm_event_channel) {
                printf("Destroying CM event channel\n");
                rdma_destroy_event_channel(cm_event_channel);
        }
}

static int setup_client()
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
        ret = rdma_create_id(cm_event_channel, &cm_client_id, NULL, RDMA_PS_TCP);
        if (ret == -1) {
                fprintf(stderr, "Creating CM id failed with errno: (%s)\n",
                                strerror(errno));
		return -errno;
        }
        printf("Client CM id is created:\n");
        print_rdma_cm_id(cm_client_id, 1);

        /* Get RDMA address for server */
        hints.ai_port_space = RDMA_PS_TCP;
        hints.ai_flags = RAI_NUMERICHOST;
        ret = rdma_getaddrinfo(server_addr, server_port, &hints, &rai);
        if (ret) {
                fprintf(stderr, "Failed rdma_getaddrinfo with errno: (%s)\n",
                                strerror(errno));
                return -errno;
        }
        printf("Successfully retrieved client's rdma_addrinfo:\n");
        print_rdma_addrinfo(rai, 1);

        /* Resolve destination and optional source addresses from IP addresses
         * to an RDMA address. If successful, the specified rdma_cm_id will be
         * bound to a local device.
         */
	ret = rdma_resolve_addr(cm_client_id, NULL, rai->ai_dst_addr, 2000);
	if (ret) {
                fprintf(stderr, "Failed rdma_resolve_addr with errno: (%s)\n",
                                strerror(errno));
		return -errno;
	}

        /* We expect the client to connect and generate a
         * RDMA_CM_EVENT_ADDR_RESOLVED. We wait (block) on the
         * CM event channel for this event.
	 */
        struct rdma_cm_event *cm_event = NULL;
        ret = process_rdma_event(cm_event_channel, &cm_event,
                                 RDMA_CM_EVENT_ADDR_RESOLVED);
        if (ret) {
                fprintf(stderr, "Failed to process CM event\n");
                return ret;
        }

        /* We got the expected RDMA_CM_EVENT_ADDR_RESOLVED event. ACK the event
         * to free the allocated memory.
         */
        printf("\nNew CM event of type %s received\n",
                rdma_event_str(cm_event->event));
        ret = rdma_ack_cm_event(cm_event);
        if (ret == -1) {
                fprintf(stderr, "Failed to ACK CM event %s: (%s)\n",
                                rdma_event_str(cm_event->event),
                                strerror(errno));
		return -errno;
        }

        /* Resolve the route to the destination address */
        int timeout_ms = 2000;
        ret = rdma_resolve_route(cm_client_id, 2000);
        if (ret == -1) {
                fprintf(stderr, "Failed to resolve route to destination within %d ms: %s\n",
                                timeout_ms,
                                strerror(errno));
		return -errno;
        }
        ret = process_rdma_event(cm_event_channel, &cm_event,
                                 RDMA_CM_EVENT_ROUTE_RESOLVED);
        if (ret) {
                fprintf(stderr, "Failed to process CM event\n");
                return ret;
        }
        /* We got the expected RDMA_CM_EVENT_ROUTE_RESOLVED event. ACK the event
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
        print_rdma_route(&cm_client_id->route, 0);
        return ret;
}

/*
 * Creates a Protection Domain (PD) using the CM id we got from the
 * client connection request earlier. That cm_id->verbs defines the
 * verbs provider of the client's RDMA device.
 *
 * Manpages: https://man7.org/linux/man-pages/man3/ibv_alloc_pd.3.html
 * RDMAmojo: https://www.rdmamojo.com/2012/08/24/ibv_alloc_pd/
 */
static int setup_protection_domain()
{
        protection_domain = ibv_alloc_pd(cm_client_id->verbs);
        if (!protection_domain) {
                fprintf(stderr, "Failed to create Protection Domain: %s\n",
                        strerror(errno));
                return -errno;
        }
        printf("Created Protection Domain:\n");
        print_ibv_pd(protection_domain, 1);
        return 0;
}

/*
 * Creates a completion channel where I/O completion notifications are sent.
 * This is different from connection management (CM) event notifications.
 * A completion channel is also tied to an RDMA device, hence we will
 * use cm_client_id->verbs.
 *
 * Manpages: https://man7.org/linux/man-pages/man3/ibv_create_comp_channel.3.html
 * RDMAmojo: https://www.rdmamojo.com/2012/10/19/ibv_create_comp_channel/
 */
static int create_completion_channel()
{
        completion_channel = ibv_create_comp_channel(cm_client_id->verbs);
        if (!completion_channel) {
                fprintf(stderr, "Failed to create Completion Channel: %s\n",
                        strerror(errno));
                return -errno;
        }
        printf("Created Completion Channel\n");
        return 0;
}

/*
 * Create a Completion Queue (CQ) where actual I/O completion metadata is
 * placed. The metadata is packed into a structure called struct ibv_wc
 * (wc = work completion). ibv_wc has detailed information about the work
 * completion. An I/O request in RDMA world is called "work".
 */
static int create_completion_queue()
{
        completion_queue = ibv_create_cq(cm_client_id->verbs, /* device */
			                 16, /* maximum capacity */
			                 NULL /* user context, not used here */,
			                 completion_channel /* IO completion channel */,
			                 0 /* Signaling vector, not used here */
                                        );
	if (!completion_queue) {
		fprintf(stderr, "Failed to create Completion Queue: %s\n",
                        strerror(errno));
                return -errno;
	}
        /* Request notifications for all WC events (option 0) */
        ibv_req_notify_cq(completion_queue, 0);
        printf("Created Completion Queue\n");
        return 0;
}

/*
 * Set up Queue Pairs (QP) and their capacities. Allocates a QP associated with
 * the specified rdma_cm_id and transition it for sending and receiving. We
 * first set up initial attributes which define the send/receive scatter-gather
 * entry (SGE) capacities.
 *
 * Manpages: https://man7.org/linux/man-pages/man3/rdma_create_qp.3.html
 * RDMAmojo: https://www.rdmamojo.com/2012/12/21/ibv_create_qp/
 */
static int setup_queue_pairs()
{
        memset(&qp_init_attr, 0, sizeof(qp_init_attr));
        qp_init_attr.cap.max_recv_sge = 2; /* Maximum SGE per receive posting */
        qp_init_attr.cap.max_recv_wr = 8; /* Maximum receive posting capacity */
        qp_init_attr.cap.max_send_sge = 2; /* Maximum SGE per send posting */
        qp_init_attr.cap.max_send_wr = 8; /* Maximum send posting capacity */
        qp_init_attr.qp_type = IBV_QPT_RC; /* QP type, RC (Reliable Connection) */

        /* We use the same completion queue for both queue pairs */
        qp_init_attr.recv_cq = completion_queue; /* Where to notify for receive completion operations */
        qp_init_attr.send_cq = completion_queue; /* Where to notify for send completion operations */

        /* Create the client QP. This will set the cm_client_id.qp field if
         * successful. After that, we'll capture that QP pointer in an external
         * static variable queue_pair.
         */
        int ret = rdma_create_qp(cm_client_id, protection_domain, &qp_init_attr);
	if (ret) {
	        fprintf(stderr, "Failed to create Queue Pair: %s\n",
                        strerror(errno));
                return -errno;
	}
        queue_pair = cm_client_id->qp;
        printf("Created client Queue Pair:\n");
        print_ibv_qp(queue_pair, 1);
        return 0;
}

/*
 * Registers the memory regions where metadata about the server's memory region
 * and remote tags will be stored, associating it with a protection domain,
 * and allowing an RDMA device to read/write to the region. This will result
 * in rkey and lkey values being generated for the RDMA device this region
 * was registered under. Creates a scatter-gather entry for where the requested
 * data. Lastly, posts the list of Work Requests (WRs)
 *
 * Manpages: https://man7.org/linux/man-pages/man3/ibv_reg_mr.3.html
 *           https://man7.org/linux/man-pages/man3/ibv_post_recv.3.html
 * RDMAmojo: https://www.rdmamojo.com/2012/09/07/ibv_reg_mr/,
 *           https://www.rdmamojo.com/2013/02/02/ibv_post_recv/
 */
static int post_metadata_recv_buffer()
{
        /* Register memory region (MR) where server metadata will be stored */
        server_metadata_mr = ibv_reg_mr(protection_domain, &server_metadata,
                                        sizeof(server_metadata),
                                        IBV_ACCESS_LOCAL_WRITE);
        if(!server_metadata_mr){
		fprintf(stderr, "Failed to register server_metadata_mr: -ENOMEM\n");
		return -ENOMEM;
	}
        printf("Successfully registered server_metadata_mr\n");
        print_ibv_mr(server_metadata_mr, 0);

        /* Associate a scatter-gather entry (SGE) with server metadata MR */
        server_recv_sge.addr = (uint64_t) server_metadata_mr->addr;
	server_recv_sge.length = (uint32_t) server_metadata_mr->length;
	server_recv_sge.lkey = server_metadata_mr->lkey;
        memset(&server_recv_wr, 0, sizeof(server_recv_wr));
        server_recv_wr.sg_list = &server_recv_sge;
	server_recv_wr.num_sge = 1;
	int ret = ibv_post_recv(queue_pair, /* the QP this is being posted to */
		                &server_recv_wr, /* receive work request */
		                &bad_server_recv_wr /* error WRs */
                               );
        if (ret) {
                fprintf(stderr, "Failed to post server_recv_wr: %s\n",
                        strerror(errno));
		return -errno;
        }
        printf("Successfully pre-posted server_recv_wr:\n");
        print_ibv_recv_wr(&server_recv_wr, 0);
        return 0;
}

/*
 * Connects to the RDMA server using rdma_connect(). This should generate a CM
 * event of type RDMA_CM_EVENT_ESTABLISHED if successful.
 *
 * Manpages: https://man7.org/linux/man-pages/man3/rdma_connect.3.html
 */
static int connect_to_server()
{
        struct rdma_conn_param conn_param;
	struct rdma_cm_event *cm_event = NULL;

        /* Before we connect we have to fill out an rdma_conn_param struct
         * containing connection properties:
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
        int ret = rdma_connect(cm_client_id, &conn_param);
        if (ret) {
                fprintf(stderr, "Failed to connect to server: %s\n",
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
        printf("Successfully connected to server RDMA device\n");
        return 0;
}

/*
 * 1. Sends our client metadata to the server, completing the server's pre-posted
 *    WR for client metadata.
 * 2. Processes the Work Completions for both our send operation, and receiving
 *    the server's metadata that we pre-posted as a WR earlier.
 *
 * Manpages: https://man7.org/linux/man-pages/man3/ibv_reg_mr.3.html
 *           https://man7.org/linux/man-pages/man3/ibv_post_send.3.html
 * RDMAmojo: https://www.rdmamojo.com/2012/09/07/ibv_reg_mr/
 *           https://www.rdmamojo.com/2013/01/26/ibv_post_send/
 */
static int exchange_metadata_with_server()
{
        /* First we need to register our source memory region, where
         * the message is stored. We're allowing the server to read/write
         * directly to it, so we need to open the access permissions.
         * Registering this MR gives us the lkey/rkey, which we'll then
         * use to satisfy the server's WR for the client metadata.
         */
        client_src_mr = ibv_reg_mr(
                protection_domain, /* Client's PD */
                src_buffer, /* Source message buffer we're registering */
                strlen(src_buffer), /* Size of message buffer, in bytes */
                (IBV_ACCESS_LOCAL_WRITE|
	         IBV_ACCESS_REMOTE_READ|
		 IBV_ACCESS_REMOTE_WRITE) /* Access flags for message */
        );
        if (!client_src_mr) {
                fprintf(stderr, "Failed to register client_src_mr: %s\n",
                        strerror(errno));
		return -errno;
        }
        printf("Registered client_src_mr:\n");
        print_ibv_mr(client_src_mr, 1);

        /* Prepare the client metadata buffer with information about the MR we
         * just registered above.
         */
	client_metadata.address = (uint64_t) client_src_mr->addr;
	client_metadata.length = client_src_mr->length;
	client_metadata.stag.local_stag = client_src_mr->lkey;
        printf("Prepared client_metadata:\n");
        print_rdma_buffer_attr(&client_metadata, 1);

        /* Register client metadata MR */
        client_metadata_mr = ibv_reg_mr(
                protection_domain, /* Client's PD */
                &client_metadata, /* Client's metadata buffer */
                sizeof(client_metadata), /* Size of client's metadata buffer */
                IBV_ACCESS_LOCAL_WRITE /* Only allow our RDMA device to write */
        );
	if (!client_metadata_mr) {
		fprintf(stderr, "Failed to register client_metadata_mr: %s\n",
                        strerror(errno));
		return -errno;
	}
        printf("Registered client_metadata_mr:\n");
        print_ibv_mr(client_metadata_mr, 1);

        /* Populate the client send SGE with information about our metadata MR
         */
	client_send_sge.addr = (uint64_t) client_metadata_mr->addr;
	client_send_sge.length = (uint32_t) client_metadata_mr->length;
	client_send_sge.lkey = client_metadata_mr->lkey;

        /* Link to the send WR. This is a SEND operation, meaning it will
         * complete some RECV WR.
         */
        bzero(&client_send_wr, sizeof(client_send_wr));
	client_send_wr.sg_list = &client_send_sge;
	client_send_wr.num_sge = 1;
	client_send_wr.opcode = IBV_WR_SEND;
	client_send_wr.send_flags = IBV_SEND_SIGNALED;

        /* Post the send WR to the client QP, containing metadata information
         * that the server requested.
         */
        int ret = ibv_post_send(
                queue_pair,
                &client_send_wr,
                &bad_client_send_wr
        );
        if (ret) {
                fprintf(stderr, "Failed to send client metadata: %s\n",
                        strerror(errno));
		return -errno;
        }
        printf("Successfully sent WR for client metadata\n");

        /* Process two WCs, one for our send, and one for receiving the
         * server's metadata that we pre-posted earlier.
         */
        int expected_wc = 2;
        struct ibv_wc work_completions[expected_wc];
        ret = process_work_completion_event(
                completion_channel,
                work_completions,
                expected_wc
        );
        if (ret != expected_wc) {
                fprintf(stderr, "Failed to process %d Work Completions: ret=%d\n",
                        expected_wc, ret);
		return ret;
        }
        printf("Got %d Work Completions\n", ret);
        printf("Now have server_metadata:\n");
        print_rdma_buffer_attr(&server_metadata, 1);

        return 0;
}

/*
 * Writes the message from the client source buffer to the remote server's
 * buffer. Since we've already gotten the server metadata through the metadata
 * exchange, we know the address, length, and rkey to use for the remote buffer.
 */
static int client_write_message()
{
        int ret = 0;

        /* Populate send SGE with information about where we're writing from */
	client_send_sge.addr = (uint64_t) client_src_mr->addr;
	client_send_sge.length = client_src_mr->length;
	client_send_sge.lkey = client_src_mr->lkey;

        /* Fill out client send WR with SGE, this tells the RDMA device what
         * data we're sending. Set opcode to WRITE (instead of SEND like before)
         * because we're writing data directly to the server's registered
         * buffer, rather than completing a recv request. In order to do this
         * direct memory write, we need to provide the remote address and rkey.
         */
        memset(&client_send_wr, 0, sizeof(client_send_wr));
	client_send_wr.sg_list = &client_send_sge;
	client_send_wr.num_sge = 1;
	client_send_wr.opcode = IBV_WR_RDMA_WRITE;
	client_send_wr.send_flags = IBV_SEND_SIGNALED;
	client_send_wr.wr.rdma.rkey = server_metadata.stag.remote_stag;
	client_send_wr.wr.rdma.remote_addr = server_metadata.address;
        printf("Prepared client_send_wr for RDMA write:\n");
        print_ibv_send_wr(&client_send_wr, 1);

        /* Send WR, effectively writing our message to server's buffer */
        ret = ibv_post_send(
                queue_pair,
		&client_send_wr,
	        &bad_client_send_wr
        );
	if (ret) {
		fprintf(stderr, "Failed to write message to server: %s\n",
                        strerror(errno));
		return -errno;
	}

        /* Now, process WC for our write */
        int expected_wc = 1;
        struct ibv_wc work_completions[expected_wc];
        ret = process_work_completion_event(
                completion_channel,
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

/* Reads the message from the remote server's buffer to a destination buffer on
 * the client. The server's buffer should have already been written to via
 * client_message_write(). Since we've already gotten the server metadata
 * through the metadata exchange, we know the address, length, and rkey to use
 * for the remote buffer.
 */
static int client_read_message()
{
        int ret = 0;

        /* First, we need to allocate enough space to read the message back.
         * (length of message + 1 for null terminator)
         */
        dst_buffer = calloc(strlen(src_buffer) + 1, sizeof(char));
        if (!dst_buffer) {
                fprintf(stderr, "Failed to allocate dst_buffer! -ENOMEM\n");
                return -ENOMEM;
        }

        /* Register dst_buffer as MR */
        client_dst_mr = ibv_reg_mr(
                protection_domain,
                dst_buffer,
                sizeof(dst_buffer),
                (IBV_ACCESS_LOCAL_WRITE|
                 IBV_ACCESS_REMOTE_WRITE|
                 IBV_ACCESS_REMOTE_READ)
        );
        if (!client_dst_mr) {
                fprintf(stderr, "Failed to register dst_buffer as MR: %s\n",
                        strerror(errno));
                return -errno;
        }

        printf("Registered dst_buffer Memory Region %p:\n", client_dst_mr);
        print_ibv_mr(client_dst_mr, 0);

        /* Populate send SGE with information about where we're writing to */
        memset(&client_send_sge, 0, sizeof(client_send_sge));
	client_send_sge.addr = (uint64_t) client_dst_mr->addr;
	client_send_sge.length = client_dst_mr->length;
	client_send_sge.lkey = client_dst_mr->lkey;

        /* Fill out client send WR with SGE, this tells the RDMA device what
         * data we're reading. Set opcode to READ because we're reading data
         * directly from the server's registered memory buffer to our own.
         * In order to do this direct memory read, we need to provide the remote
         * address and rkey.
         */
        memset(&client_send_wr, 0, sizeof(client_send_wr));
        client_send_wr.sg_list = &client_send_sge;
	client_send_wr.num_sge = 1;
	client_send_wr.opcode = IBV_WR_RDMA_READ;
	client_send_wr.send_flags = IBV_SEND_SIGNALED;
	client_send_wr.wr.rdma.rkey = server_metadata.stag.remote_stag;
	client_send_wr.wr.rdma.remote_addr = server_metadata.address;

        /* Send WR, effectively reading the message from the server's buffer */
        ret = ibv_post_send(
                queue_pair,
		&client_send_wr,
	        &bad_client_send_wr
        );
	if (ret) {
		fprintf(stderr, "Failed to read message from server: %s\n",
                        strerror(errno));
		return -errno;
	}

        /* Now, process WC for our write */
        int expected_wc = 1;
        struct ibv_wc work_completions[expected_wc];
        ret = process_work_completion_event(
                completion_channel,
                work_completions,
                expected_wc
        );
        if (ret != expected_wc) {
                fprintf(stderr, "Failed to process %d Work Completions: ret=%d\n",
                        expected_wc, ret);
		return ret;
        }
        printf("Got %d Work Completions\n", ret);

        /* Make sure to null-terminate the dst buffer before printing it */
        dst_buffer[strlen(src_buffer)] = '\0';
        printf("Client read complete. dst_buffer contents: '%s'\n", dst_buffer);
        return ret;
}

static void print_usage()
{
        printf("Usage:\n\t./rdma-client -m <message> -s <server_host> -p <server_port>\n");
        printf("Example:\n\t./rdma-client -m \"hello\" -s 192.168.0.105 -p 20021\n");
}

int main(int argc, char **argv)
{

        int option;
        size_t message_len;
        while ((option = getopt(argc, argv, "m:s:p:")) != -1) {
                switch (option) {
                        case 'm':
                                /* Allocate some space for our message */
                                message_len = strlen(optarg);
                                src_buffer = calloc(message_len + 1, sizeof(char));
                                if (!src_buffer) {
                                        fprintf(stderr, "Failed to allocate memory for message\n");
                                        return -ENOMEM;
                                }

                                /* Copy the passed argument from optarg to our
                                 * allocated message buffer. We'll free it when
                                 * we clean up the client resources.
                                 */
                                strncpy(src_buffer, optarg, message_len);
                                printf("src_buffer contents: '%s'\n",
                                       src_buffer);
                                break;
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

        if (!src_buffer) {
                printf("Please provide a string message to send/recv\n");
                print_usage();
                return 1;
        }

        int ret = setup_client();
        if (ret) {
                cleanup_client();
                return ret;
        }

        ret = setup_protection_domain();
        if (ret) {
                cleanup_client();
                return ret;
        }

        ret = create_completion_channel();
        if (ret) {
                cleanup_client();
                return ret;
        }

        ret = create_completion_queue();
        if (ret) {
                cleanup_client();
                return ret;
        }

        ret = setup_queue_pairs();
        if (ret) {
                cleanup_client();
                return ret;
        }

        ret = post_metadata_recv_buffer();
        if (ret) {
                cleanup_client();
                return ret;
        }

        ret = connect_to_server();
        if (ret) {
                cleanup_client();
                return ret;
        }

        ret = exchange_metadata_with_server();
        if (ret) {
                cleanup_client();
                return ret;
        }

        ret = client_write_message();
        if (ret) {
                cleanup_client();
                return ret;
        }

        ret = client_read_message();
        if (ret) {
                cleanup_client();
                return ret;
        }

        cleanup_client();

        return 0;
}