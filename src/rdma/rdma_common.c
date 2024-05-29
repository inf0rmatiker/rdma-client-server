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

        struct flag_str ai_flag_pairs[] = {
                {RAI_PASSIVE, "RAI_PASSIVE"},
                {RAI_NUMERICHOST, "RAI_NUMERICHOST"},
                {RAI_NOROUTE, "RAI_NOROUTE"},
                {RAI_FAMILY, "RAI_FAMILY"}
        };

        char ai_flags_str[64] = { 0 };
        bitflags_to_str(ai_flag_pairs, 4, rai->ai_flags, ai_flags_str);
        printf("\tai_flags: %s\n", ai_flags_str);
        printf("}\n");
}
