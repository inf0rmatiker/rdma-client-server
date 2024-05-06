#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <getopt.h>
#include <netdb.h>
#include <rdma/rdma_cma.h>
#include <rdma/rdma_verbs.h>

int main(int argc, char **argv) {

        /* Create CM event channel for asynchronous communication events */
        struct rdma_event_channel *cm_event_channel = rdma_create_event_channel();
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
		return -errno;
        }
        printf("Successfully bound RDMA server address 0.0.0.0:20021\n");


        /* Destroy server connection identifier */
        ret = rdma_destroy_id(cm_server_id);
	if (ret == -1) {
		fprintf(stderr, "Failed to destroy server CM id: (%s)\n",
                        strerror(errno));
	}

        /* Clean-up and destroy CM event channel */
        rdma_destroy_event_channel(cm_event_channel);
	printf("Successfully destroyed CM event channel\n");
        return 0;
}