/*
 * rdma_common.h defines common functions, variables, and definitions
 * used by both the client and server sources.
 */

#ifndef RDMA_COMMON_H
#define RDMA_COMMON_H

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
#include <rdma/rsocket.h>
#include <infiniband/ib.h>
#include <infiniband/verbs.h>


/*
 * Mapping between a bitflag and its enum string.
 */
struct flag_str {
        int value;
        const char* str;
};

/*
 * Converts a set of bitflags to a human-readable string.
 * If there are more than 1 flags set, they are separated by the '|' character.
 * Stores the string result in *res pointer passed in.
 * Example: "RAI_PASSIVE | RAI_NUMERICHOST"
 */
void bitflags_to_str(struct flag_str *pairs, size_t count, int flags, char *res);

/*
 * Prints an rdma_addrinfo struct in human-readable terms.
 */
void print_rdma_addrinfo(const struct rdma_addrinfo* rai);

/*
 * process_rdma_event fully checks and processes an RDMA event on the
 * Connection Manager event channel.
 */
int process_rdma_event(struct rdma_event_channel *ec,
                       struct rdma_cm_event **event,
                       enum rdma_cm_event_type type);


#endif /* RDMA_COMMON_H */