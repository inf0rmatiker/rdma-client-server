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

static char *server_addr = "127.0.0.1";
static char *server_port = "7471";
static char *message = NULL;

/* Connection manager data structures for client */
static struct rdma_event_channel *cm_event_channel;
static struct rdma_cm_id *cm_client_id, *cm_server_id;
static struct rdma_addrinfo *rai, hints;

/* Memory registration */
static struct ibv_mr *mr, *send_mr;



void cleanup_client()
{
        if (message) {
                free(message);
        }

        if (rai) {
                printf("Freeing rai rdma_addrinfo\n");
                rdma_freeaddrinfo(rai);
        }

        if (cm_client_id) {
                printf("Destroying rdma_cm_id cm_client_id\n");
                rdma_destroy_ep(cm_client_id);
        }

        if (cm_server_id) {
                printf("Destroying rdma_cm_id cm_server_id\n");
                rdma_destroy_ep(cm_server_id);
        }

        if (cm_event_channel) {
                printf("Destroying CM event channel\n");
                rdma_destroy_event_channel(cm_event_channel);
        }
}

int setup_client()
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
        printf("Client CM id is created\n");
        print_rdma_cm_id(cm_client_id);

        /* Get RDMA address for server */
        hints.ai_port_space = RDMA_PS_TCP;
        hints.ai_flags = RAI_NUMERICHOST;
        ret = rdma_getaddrinfo(server_addr, server_port, &hints, &rai);
        if (ret) {
                fprintf(stderr, "Failed rdma_getaddrinfo with errno: (%s)\n",
                                strerror(errno));
                return -errno;
        }
        printf("Successfully retrieved rdma_addrinfo\n");
        print_rdma_addrinfo(rai);

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
         * RDMA_CM_EVENT_CONNECT_REQUEST. We wait (block) on the
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
        printf("New CM event of type %s received\n",
                rdma_event_str(cm_event->event));
        ret = rdma_ack_cm_event(cm_event);
        if (ret == -1) {
                fprintf(stderr, "Failed to ACK CM event %s: (%s)\n",
                                rdma_event_str(cm_event->event),
                                strerror(errno));
		return -errno;
        }

        return ret;
}

void print_usage()
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
                                message = calloc(message_len + 1, sizeof(char));
                                if (!message) {
                                        fprintf(stderr, "Failed to allocate memory for message\n");
                                        return -ENOMEM;
                                }

                                /* Copy the passed argument from optarg to our
                                 * allocated message buffer. We'll free it when
                                 * we clean up the client resources.
                                 */
                                strncpy(message, optarg, message_len);
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

        if (!message) {
                printf("Please provide a string message to send/recv\n");
                print_usage();
                return 1;
        }

        int ret = setup_client();
        cleanup_client();

        return 0;
}