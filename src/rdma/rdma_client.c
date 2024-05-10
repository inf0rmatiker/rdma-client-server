#include "rdma_common.h"

/* Connection manager data structures for client */
static struct rdma_event_channel *cm_event_channel;
static struct rdma_cm_id *cm_client_id, *cm_server_id;
static struct rdma_addrinfo *res, hints;

/* Memory registration */
static struct ibv_mr *mr, *send_mr;

void print_usage()
{
        printf("Usage:\n\t./rdma-client <client_host> <server_host> <server_port>\n");
        printf("Example:\n\t./rdma-client 192.168.0.106 192.168.0.105 20021\n");
}

void cleanup_client()
{
        rdma_destroy_event_channel(cm_event_channel);
        rdma_freeaddrinfo(res);
        rdma_destroy_ep(cm_server_id);
}

int run()
{
	memset(&hints, 0, sizeof hints);
	hints.ai_port_space = RDMA_PS_TCP;
	int ret = rdma_getaddrinfo("192.168.0.106", "20021", &hints, &res);
	if (ret) {
		fprintf(stderr, "Failed rdma_getaddrinfo with errno: (%s)\n",
                                strerror(errno));
                cleanup_client();
                return -errno;
	}

        struct ibv_qp_init_attr init_attr;
        struct ibv_qp_attr qp_attr;
        struct ibv_wc wc;
        memset(&init_attr, 0, sizeof init_attr);
        init_attr.cap.max_send_wr = init_attr.cap.max_recv_wr = 1;
        init_attr.cap.max_send_sge = init_attr.cap.max_recv_sge = 1;
        init_attr.cap.max_inline_data = 40;
        init_attr.sq_sig_all = 1;
        init_attr.qp_context = cm_server_id;
        ret = rdma_create_ep(&cm_server_id, res, NULL, &init_attr);
        if (ret) {
                fprintf(stderr, "Failed rdma_create_ep with errno: (%s)\n",
                                strerror(errno));
                cleanup_client();
                return -errno;
        }

        printf("max_inline_data=%d\n", init_attr.cap.max_inline_data);

        int send_flags = IBV_SEND_INLINE;

        char recv_msg[40];
        char send_msg[40];
        mr = rdma_reg_msgs(cm_server_id, recv_msg, 40);
        if (!mr) {
                fprintf(stderr, "Failed register receive buffer\n");
                rdma_dereg_mr(mr);
                cleanup_client();
                return -1;
        }

        send_mr = rdma_reg_msgs(cm_server_id, send_msg, 40);
        if (!send_mr) {
                fprintf(stderr, "Failed register send buffer\n");
                rdma_dereg_mr(mr);
                rdma_dereg_mr(send_mr);
                cleanup_client();
                return -1;
        }
        printf("Successfully registered both recv_msg and send_msg buffers\n");

        // ret = rdma_post_recv(cm_server_id, NULL, recv_msg, 40, mr);
        // if (ret) {
        //         fprintf(stderr, "Failed rdma_post_rcv\n");
        //         rdma_dereg_mr(mr);
        //         rdma_dereg_mr(send_mr);
        //         cleanup_client();
        //         return -1;
        // }
        // printf("Successfully posted recv buffer\n");

        ret = rdma_connect(cm_server_id, NULL);
        if (ret) {
                fprintf(stderr, "Failed rdma_connect\n");
                rdma_dereg_mr(mr);
                rdma_dereg_mr(send_mr);
                cleanup_client();
                return -1;
        }
        printf("Successfully connected\n");


        rdma_disconnect(cm_server_id);
        rdma_dereg_mr(mr);
        rdma_dereg_mr(send_mr);
        cleanup_client();
        return 0;
}

int main(int argc, char **argv)
{

        if (argc < 3) {
                print_usage();
                return 1;
        }

        const char *client_host = argv[1];
        const char *server_host = argv[2];
        int server_port = atoi(argv[3]);

        return run();

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
                                 RDMA_PS_IPOIB);
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
        server_sockaddr.sin_addr.s_addr = inet_addr(server_host);
        server_sockaddr.sin_port = htons(server_port);

        /* Optional: set up client sockaddr_in information */
        struct sockaddr_in client_sockaddr;
        memset(&client_sockaddr, 0, sizeof(client_sockaddr));
        client_sockaddr.sin_family = AF_INET;
        client_sockaddr.sin_addr.s_addr = inet_addr(client_host);

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
        ret = process_rdma_event(cm_event_channel, &cm_event,
                                 RDMA_CM_EVENT_ADDR_RESOLVED);
        if (ret) {
                fprintf(stderr, "Failed to process CM event\n");
                cleanup_client();
                return ret;
        }
        ret = rdma_get_cm_event(cm_event_channel, &cm_event);

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

        /* Resolve the RDMA route to destination address for our connection */
        ret = rdma_resolve_route(cm_client_id, timeout_ms);
        if (ret) {
                fprintf(stderr, "Failed to resolve RDMA route: (%s)\n",
                        strerror(errno));
                cleanup_client();
                return -errno;
        }

        /* Wait for CM event for route resolution */
        ret = process_rdma_event(cm_event_channel, &cm_event,
                                 RDMA_CM_EVENT_ROUTE_RESOLVED);
        if (ret) {
                fprintf(stderr, "Failed to process CM event\n");
                cleanup_client();
                return ret;
        }
        printf("New CM event of type %s received\n",
                rdma_event_str(cm_event->event));
        ret = rdma_ack_cm_event(cm_event);
        if (ret) {
                fprintf(stderr, "Failed to ack the RDMA_CM_EVENT_ROUTE_RESOLVED event: (%s)\n",
                                strerror(errno));
                cleanup_client();
                return -errno;
        }
        printf("Acknowledged RDMA_CM_EVENT_ROUTE_RESOLVED event\n");


        printf("Trying to connect to server at : %s port: %d \n",
	       inet_ntoa(server_sockaddr.sin_addr),
	       ntohs(server_sockaddr.sin_port));


        printf("Cleaning up client and exiting\n");
        cleanup_client();
        return 0;
}