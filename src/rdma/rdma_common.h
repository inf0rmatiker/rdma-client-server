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
#include <sys/socket.h>
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
 * This struct is used to exchange information about buffers between the server
 * and clients.
 *
 * Note: Use the packed attribute so that compiler does not step in and try to
 * pad the struct. For details see:
 * http://gcc.gnu.org/onlinedocs/gcc/Type-Attributes.html
 */
struct __attribute((packed)) rdma_buffer_attr {
  uint64_t address;
  uint32_t length;
  union stag {
	  /* if we send, we call it local stags */
	  uint32_t local_stag;
	  /* if we receive, we call it remote stag */
	  uint32_t remote_stag;
  } stag;
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
 * Prints an rdma_cm_id struct in human-readable terms.
 */
void print_rdma_cm_id(const struct rdma_cm_id* cm_id);

/*
 * Prints an ibv_device struct in human-readable terms.
 */
void print_ibv_device(const struct ibv_device* device);

/*
 * Prints an ibv_mr struct in human-readable terms.
 */
void print_ibv_mr(const struct ibv_mr *mr);

/*
 * Prints an ibv_sge struct in human-readable terms.
 */
void print_ibv_sge(const struct ibv_sge *sge);

/*
 * Prints an ibv_recv_wr struct in human-readable terms.
 */
void print_ibv_recv_wr(const struct ibv_recv_wr *);

/*
 * Prints an rdma_buffer_attr struct in human-readable terms.
 */
void print_rdma_buffer_attr(const struct rdma_buffer_attr *);

/*
 * Prints a sockaddr struct in human-readable terms.
 */
void print_sockaddr(const struct sockaddr *);

/*
 * Prints an rdma_route struct in human-readable terms.
 */
void print_rdma_route(const struct rdma_route *);

/*
 * Prints an rdma_addr struct in human-readable terms.
 */
void print_rdma_addr(const struct rdma_addr *);

/*
 * process_rdma_event fully checks and processes an RDMA event on the
 * Connection Manager event channel.
 */
int process_rdma_event(struct rdma_event_channel *ec,
                       struct rdma_cm_event **event,
                       enum rdma_cm_event_type type);

#endif /* RDMA_COMMON_H */