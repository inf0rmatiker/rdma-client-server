#include "rdma_common.h"

int process_rdma_event(struct rdma_event_channel *event_channel,
                       struct rdma_cm_event **event,
                       enum rdma_cm_event_type expected_type)
{
        /* Block until we receive a communication event */
        int ret = rdma_get_cm_event(event_channel, event);
        if (ret) {
                fprintf(stderr, "Blocking for CM events failed: (%s)\n",
                                strerror(errno));
                return -errno;
        }

        /* Check the status of the event */
        if ((*event)->status != 0) {
                fprintf(stderr, "CM event received with non-zero status: (%d)\n",
                                (*event)->status);

                /* Even if we get a bad status we still need to ACK the event */
                rdma_ack_cm_event(*event);
                return -1;
        }

        /* Check the type of event is what we expect */
        if ((*event)->event != expected_type) {
                fprintf(stderr, "CM event received with unexpected type. Expected %s, but got %s\n",
                                rdma_event_str(expected_type),
                                rdma_event_str((*event)->event));
                /* Even if we got unexpected event type we still need to ACK */
                rdma_ack_cm_event((*event));
                return -1;
        }

        return 0;
}

void bitflags_to_str(struct flag_str *pairs, size_t count, int flags, char *res)
{
	int has_flag = 0;
	for (int i = 0; i < count; i++) {
		if (flags & (pairs[i]).value) {
			if (has_flag) {
				strcat(res, " | ");
			}
			strcat(res, pairs[i].str);
			has_flag = 1;
		}
	}
	strcat(res, "\0");
	return;
}

void print_rdma_addrinfo(const struct rdma_addrinfo* rai)
{
        printf("rdma_addrinfo{\n");

        struct flag_str ai_flags[] = {
                {RAI_PASSIVE, "RAI_PASSIVE"},
                {RAI_NUMERICHOST, "RAI_NUMERICHOST"},
                {RAI_NOROUTE, "RAI_NOROUTE"},
                {RAI_FAMILY, "RAI_FAMILY"}
        };
        char ai_flags_str[64] = { 0 };
        bitflags_to_str(ai_flags, 4, rai->ai_flags, ai_flags_str);
        printf("\tai_flags: %s\n", ai_flags_str);

        struct flag_str ai_families[] = {
                {AF_INET, "AF_INET"},
                {AF_INET6, "AF_INET6"},
                {AF_IB, "AF_IB"}
        };
        char ai_family_str[64] = { 0 };
        bitflags_to_str(ai_families, 3, rai->ai_family, ai_family_str);
        printf("\tai_family: %s\n", ai_family_str);

        struct flag_str ai_qp_types[] = {
                {IBV_QPT_UD, "IBV_QPT_UD"},
                {IBV_QPT_RC, "IBV_QPT_RC"}
        };
        char ai_qp_types_str[32] = { 0 };
        bitflags_to_str(ai_qp_types, 3, rai->ai_qp_type, ai_qp_types_str);
        printf("\tai_qp_type: %s\n", ai_qp_types_str);

        struct flag_str ai_ps_types[] = {
                {RDMA_PS_UDP, "RDMA_PS_UDP"},
                {RDMA_PS_IPOIB, "RDMA_PS_IPOIB"},
                {RDMA_PS_TCP, "RDMA_PS_TCP"},
                {RDMA_PS_IB, "RDMA_PS_IB"}
        };
        char rdma_ps_str[64] = { 0 };
        bitflags_to_str(ai_ps_types, 3, rai->ai_port_space, rdma_ps_str);
        printf("\tai_port_space: %s\n", rdma_ps_str);

        printf("\tai_src_len: %d\n", rai->ai_src_len);
        printf("\tai_dst_len: %d\n", rai->ai_dst_len);
        printf("\tai_src_addr: { struct sockaddr* %p }\n", rai->ai_src_addr);
        printf("\tai_dst_addr: { struct sockaddr* %p }\n", rai->ai_dst_addr);
        printf("\tai_src_canonname: %s\n", rai->ai_src_canonname);
        printf("\tai_dst_canonname: %s\n", rai->ai_dst_canonname);
        printf("\tai_route_len: %d\n", rai->ai_route_len);
        printf("\tai_route: { void* %p }\n", rai->ai_route);
        printf("\tai_connect_len: %d\n", rai->ai_connect_len);
        printf("\tai_connect: { void* %p }\n", rai->ai_connect);
        printf("\tai_next: { struct rdma_addrinfo* %p }\n", rai->ai_next);
        printf("}\n");
}
