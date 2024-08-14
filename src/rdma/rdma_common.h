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
void print_rdma_addrinfo(const struct rdma_addrinfo *, int);

/*
 * Prints an rdma_cm_id struct in human-readable terms.
 */
void print_rdma_event_channel(const struct rdma_event_channel *, int);

/*
 * Prints an rdma_cm_id struct in human-readable terms.
 */
void print_rdma_cm_id(const struct rdma_cm_id *, int);

/*
 * Prints an ibv_context struct in human-readable terms.
 */
void print_ibv_context(const struct ibv_context *, int);

/*
 * Prints an ibv_device struct in human-readable terms.
 */
void print_ibv_device(const struct ibv_device *, int);

/*
 * Prints an ibv_mr struct in human-readable terms.
 */
void print_ibv_mr(const struct ibv_mr *, int);

/*
 * Prints an ibv_qp struct in human-readable terms.
 */
void print_ibv_qp(const struct ibv_qp *, int);

/*
 * Prints an ibv_pd struct in human-readable terms.
 */
void print_ibv_pd(const struct ibv_pd *, int);

/*
 * Prints an ibv_sge struct in human-readable terms.
 */
void print_ibv_sge(const struct ibv_sge *, int);

/*
 * Prints an ibv_recv_wr struct in human-readable terms.
 */
void print_ibv_recv_wr(const struct ibv_recv_wr *, int);

/*
 * Prints an ibv_send_wr struct in human-readable terms.
 */
void print_ibv_send_wr(const struct ibv_send_wr *, int);

/*
 * Prints an rdma_buffer_attr struct in human-readable terms.
 */
void print_rdma_buffer_attr(const struct rdma_buffer_attr *, int);

/*
 * Prints a sockaddr struct in human-readable terms.
 */
void print_sockaddr(const struct sockaddr *, int);

/*
 * Prints an rdma_route struct in human-readable terms.
 */
void print_rdma_route(const struct rdma_route *, int);

/*
 * Prints an rdma_addr struct in human-readable terms.
 */
void print_rdma_addr(const struct rdma_addr *, int);

/*
 * process_rdma_event fully checks and processes an RDMA event on the
 * Connection Manager event channel.
 *
 * event_channel: pointer to the CM event channel.
 * event: pointer to the event pointer.
 * expected_type: expected enum type of the RDMA CM event.
 */
int process_rdma_event(struct rdma_event_channel *event_channel,
                       struct rdma_cm_event **event,
                       enum rdma_cm_event_type expected_type);

/*
 * process_work_completion_event processes expected_wc Work Completion events
 * on the completion_channel IO Completion Channel. WC elements are stored in
 * the ibv_wc array starting at the wc pointer.
 *
 * Returns the total number of WC elements successfully retrieved from the CQ.
 *
 * Manpages: https://man7.org/linux/man-pages/man3/ibv_ack_cq_events.3.html
 *           https://linux.die.net/man/3/ibv_req_notify_cq
 *           https://man7.org/linux/man-pages/man3/ibv_poll_cq.3.html
 *           https://linux.die.net/man/3/ibv_ack_cq_events
 * RDMAmojo: https://www.rdmamojo.com/2013/03/09/ibv_get_cq_event/
 *           https://www.rdmamojo.com/2013/02/22/ibv_req_notify_cq/
 *           https://www.rdmamojo.com/2013/02/15/ibv_poll_cq/
 *           https://www.rdmamojo.com/2013/03/16/ibv_ack_cq_events/
 */
int process_work_completion_event(struct ibv_comp_channel *completion_channel,
                                  struct ibv_wc *wc,
                                  int expected_wc);

/*
 * Creates and registers a buffer of size size_bytes as a Memory Region under
 * the pd Protection Domain.
 *
 * Returns an ibv_mr pointer if successful, NULL otherwise.
 */
struct ibv_mr *create_rdma_buffer(struct ibv_pd *pd, uint32_t size_bytes,
                         enum ibv_access_flags perms);

#endif /* RDMA_COMMON_H */