/*
 * rdma_common.h defines common functions, variables, and definitions
 * used by both the client and server sources.
 */

#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <rdma/rdma_cma.h>

/*
 * process_rdma_event fully checks and processes an RDMA event on the
 * Connection Manager event channel.
 */
int process_rdma_event(struct rdma_event_channel *ec,
                       struct rdma_cm_event **event,
                       enum rdma_cm_event_type type);



#endif /* RDMA_COMMON_H */