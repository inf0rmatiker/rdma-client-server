#include "rdma_common.h"

static char *server_addr = "127.0.0.1";
static char *server_port = "7471";

/* Connection manager data structures for server */
static struct rdma_event_channel *cm_event_channel;
static struct rdma_cm_id *cm_server_id, *cm_client_id;
static struct rdma_addrinfo *rai, hints;

void print_usage()
{
        printf("Usage:\n\t./rdma-server <server_host> <server_port>\n");
        printf("Example:\n\t./rdma-server 192.168.0.106 20021\n");
}

/* Cleans up Connection Manager ID and Event Channel objects before exiting.
 */
void cleanup_server()
{
        /* Destroy client and server connection identifiers */
        rdma_destroy_ep(cm_client_id);
        rdma_destroy_ep(cm_server_id);

        /* Clean-up and destroy CM event channel */
        rdma_destroy_event_channel(cm_event_channel);
        rdma_freeaddrinfo(rai);
	printf("Successfully destroyed CM event channel\n");
}

int run()
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
        struct rdma_cm_id *cm_server_id = NULL;
        ret = rdma_create_id(cm_event_channel, &cm_server_id, NULL, RDMA_PS_TCP);
        if (ret == -1) {
                fprintf(stderr, "Creating CM id failed with errno: (%s)\n",
                                strerror(errno));
		return -errno;
        }
        printf("Server CM id is created\n");

        /* Figure out the rdma_addrinfo of our RDMA device */
        memset(&hints, 0, sizeof(hints));
        hints.ai_flags = RAI_NUMERICHOST | RAI_PASSIVE;
        hints.ai_port_space = RDMA_PS_TCP;
        ret = rdma_getaddrinfo(server_addr, server_port, &hints, &rai);
        if (ret) {
                fprintf(stderr, "Failed rdma_getaddrinfo with errno: (%s)\n",
                                strerror(errno));
                cleanup_server();
                return -errno;
        }
        printf("Successfully retrieved rdma_addrinfo\n");
        print_rdma_addrinfo(rai);

        /*
        struct ibv_qp_init_attr init_attr;
        struct ibv_qp_attr qp_attr;
        struct ibv_wc wc;
        memset(&init_attr, 0, sizeof init_attr);
        init_attr.cap.max_send_wr = init_attr.cap.max_recv_wr = 1;
        init_attr.cap.max_send_sge = init_attr.cap.max_recv_sge = 1;
        init_attr.cap.max_inline_data = 40;
        init_attr.sq_sig_all = 1;
        ret = rdma_create_ep(&cm_server_id, res, NULL, &init_attr);
        if (ret) {
                fprintf(stderr, "Failed rdma_create_ep with errno: (%s)\n",
                                strerror(errno));
                cleanup_server();
                return -errno;
        }

        ret = rdma_listen(cm_server_id, 0);
        if (ret) {
                fprintf(stderr, "Failed rdma_listen with errno: (%s)\n",
                                strerror(errno));
                cleanup_server();
                return -errno;
        }

        ret = rdma_get_request(cm_server_id, &cm_client_id);
        if (ret) {
                fprintf(stderr, "Failed rdma_get_request with errno: (%s)\n",
                                strerror(errno));
                cleanup_server();
                return -errno;
        }
        printf("Got request\n");


        rdma_disconnect(cm_client_id);
        rdma_destroy_ep(cm_client_id);
        rdma_destroy_ep(cm_server_id);
        rdma_freeaddrinfo(res);

        */

        rdma_freeaddrinfo(rai);
        rdma_destroy_event_channel(cm_event_channel);

        // printf("res = rdma_addrinfo {"
        //        "ai_family=%d\n"
        //        "ai_qp_type=%d\n"
        //        "ai_src_canonname=%s\n"
        //        "ai_dst_canonname=%s\n"
        //        "ai_route_len=%d\n"
        //        "}\n",
        //        res->ai_family,
        //        res->ai_qp_type,
        //        res->ai_src_canonname,
        //        res->ai_dst_canonname,
        //        res->ai_route_len
        // );
        return 0;
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

        return run();

        /* Bind to an RDMA address. To do this, we need to create a sockaddr_in,
         * fill out the fields, then cast it to a sockaddr* before passing it to
         * rdma_bind_addr.
         */
        // struct sockaddr_in server_sockaddr;
        // memset(&server_sockaddr, 0, sizeof(server_sockaddr));
	// server_sockaddr.sin_family = AF_INET; /* standard IP NET address */
	// server_sockaddr.sin_addr.s_addr = inet_addr(server_host); /* passed address */
        // server_sockaddr.sin_port = htons(server_port);

        // ret = rdma_bind_addr(cm_server_id, (struct sockaddr *)&server_sockaddr);
        // if (ret == -1) {
        //         fprintf(stderr, "Binding server RDMA address failed with errno: (%s)\n",
        //                         strerror(errno));
        //         cleanup_server();
	// 	return -errno;
        // }
        // printf("Successfully bound RDMA server address %s:%d\n", server_host,
        //        server_port);

        /* Initiate a listen on the RDMA IP address and port.
         * This is a non-blocking call.
         */
        // ret = rdma_listen(cm_server_id, 8);
        // if (ret == -1) {
        //         fprintf(stderr, "Listening for CM events failed: (%s)\n",
        //                         strerror(errno));
        //         cleanup_server();
	// 	return -errno;
        // }
        // printf("Server is listening successfully at: %s, port: %d \n",
        //        inet_ntoa(server_sockaddr.sin_addr),
	//        ntohs(server_sockaddr.sin_port));

        /* We expect the client to connect and generate a
         * RDMA_CM_EVENT_CONNECT_REQUEST. We wait (block) on the
         * connection management event channel for this event.
	 */
        // struct rdma_cm_event *cm_event = NULL;
        // ret = process_rdma_event(cm_event_channel, &cm_event,
        //                          RDMA_CM_EVENT_CONNECT_REQUEST);
        // if (ret) {
        //         fprintf(stderr, "Failed to process CM event\n");
        //         cleanup_server();
        //         return ret;
        // }

        /* We got the expected RDMA_CM_EVENT_CONNECT_REQUEST event */
        // printf("New CM event of type %s received\n",
        //         rdma_event_str(cm_event->event));

        /* Much like TCP connection, listening returns a new connection
         * id for a newly connected client. In the case of RDMA, this is stored
         * in the cm_event->id field. We need to save this information before
         * acknowledging the event, which also frees the struct.
	 */
	// cm_client_id = cm_event->id;
        // ret = rdma_ack_cm_event(cm_event);
        // if (ret == -1) {
        //         fprintf(stderr, "Failed to ACK CM event %s: (%s)\n",
        //                         rdma_event_str(cm_event->event),
        //                         strerror(errno));
        //         rdma_destroy_id(cm_client_id);
        //         cleanup_server();
	// 	return -errno;
        // }
        // printf("New RDMA connection stored at %p\n", cm_client_id);

        /* Cleanup before exiting */
        // printf("Cleaning up server and exiting\n");
        // rdma_destroy_id(cm_client_id);
        // cleanup_server();
        // return 0;
}