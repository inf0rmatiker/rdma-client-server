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



        /* Clean-up and destroy CM event channel */
        rdma_destroy_event_channel(cm_event_channel);
	printf("Successfully destroyed CM event channel\n");
        return 0;
}