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

static void print_bits(int value) {
        for (int i = ((sizeof(value) * 8) - 1); i >= 0; i--) {
                printf("%u", (value >> i) & 1);
        }
        printf("\n");
}

void rdma_ps_str(enum rdma_port_space value, char *res) {
        switch (value) {
                case RDMA_PS_IPOIB:
                        strcat(res, "RDMA_PS_IPOIB");
                        break;
                case RDMA_PS_TCP:
                        strcat(res, "RDMA_PS_TCP");
                        break;
                case RDMA_PS_IB:
                        strcat(res, "RDMA_PS_IB");
                        break;
                case RDMA_PS_UDP:
                        strcat(res, "RDMA_PS_UDP");
                        break;
                default:
                        strcat(res, "Unknown");
        }
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
        // struct rdma_addrinfo {
        //         int			ai_flags;
        //         int			ai_family;
        //         int			ai_qp_type;
        //         int			ai_port_space;
        //         socklen_t		ai_src_len;
        //         socklen_t		ai_dst_len;
        //         struct sockaddr	*ai_src_addr;
        //         struct sockaddr	*ai_dst_addr;
        //         char			*ai_src_canonname;
        //         char			*ai_dst_canonname;
        //         size_t		ai_route_len;
        //         void			*ai_route;
        //         size_t		ai_connect_len;
        //         void			*ai_connect;
        //         struct rdma_addrinfo	*ai_next;
        // };

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

void print_ibv_device(const struct ibv_device* device)
{
        // struct ibv_device {
        //         struct _ibv_device_ops	_ops;
        //         enum ibv_node_type	        node_type;
        //         enum ibv_transport_type	transport_type;
        //         /* Name of underlying kernel IB device, eg "mthca0" */
        //         char			        name[IBV_SYSFS_NAME_MAX];
        //         /* Name of uverbs device, eg "uverbs0" */
        //         char			        dev_name[IBV_SYSFS_NAME_MAX];
        //         /* Path to infiniband_verbs class device in sysfs */
        //         char			        dev_path[IBV_SYSFS_PATH_MAX];
        //         /* Path to infiniband class device in sysfs */
        //         char			        ibdev_path[IBV_SYSFS_PATH_MAX];
        // };

        if (!device) {
                printf("(null)\n");
                return;
        }

        printf("ibv_device{\n");

        struct flag_str ibv_node_types[] = {
                {IBV_NODE_UNKNOWN, "IBV_NODE_UNKNOWN"},
                {IBV_NODE_CA, "IBV_NODE_CA"},
                {IBV_NODE_SWITCH, "IBV_NODE_SWITCH"},
                {IBV_NODE_ROUTER, "IBV_NODE_ROUTER"},
                {IBV_NODE_RNIC, "IBV_NODE_RNIC"},
                {IBV_NODE_USNIC, "IBV_NODE_USNIC"},
                {IBV_NODE_USNIC_UDP, "IBV_NODE_USNIC_UDP"},
                {IBV_NODE_UNSPECIFIED, "IBV_NODE_UNSPECIFIED"}
        };
        char node_type_str[32] = { 0 };
        bitflags_to_str(ibv_node_types, 8, (int)device->node_type, node_type_str);
        printf("\tnode_type: %s\n", node_type_str);

        struct flag_str ibv_transport_types[] = {
                {IBV_TRANSPORT_UNKNOWN, "IBV_TRANSPORT_UNKNOWN"},
                {IBV_TRANSPORT_IB, "IBV_TRANSPORT_IB"},
                {IBV_TRANSPORT_IWARP, "IBV_TRANSPORT_IWARP"},
                {IBV_TRANSPORT_USNIC, "IBV_TRANSPORT_USNIC"},
                {IBV_TRANSPORT_USNIC_UDP, "IBV_TRANSPORT_USNIC_UDP"}
        };
        char transport_type_str[32] = { 0 };
        bitflags_to_str(ibv_transport_types, 5, (int)device->transport_type,
                    transport_type_str);
        printf("\transport_type: %s\n", transport_type_str);

        printf("\tname: %s\n", device->name);
        printf("\tdev_name: %s\n", device->dev_name);
        printf("\tdev_path: %s\n", device->dev_path);
        printf("\tibdev_path: %s\n", device->ibdev_path);
        printf("}\n");
}

void print_rdma_cm_id(const struct rdma_cm_id* cm_id)
{
        // struct rdma_cm_id {
        //         struct ibv_context	        *verbs;
        //         struct rdma_event_channel    *channel;
        //         void			        *context;
        //         struct ibv_qp		*qp;
        //         struct rdma_route	        route;
        //         enum rdma_port_space	        ps;
        //         uint8_t			port_num;
        //         struct rdma_cm_event	        *event;
        //         struct ibv_comp_channel      *send_cq_channel;
        //         struct ibv_cq		*send_cq;
        //         struct ibv_comp_channel      *recv_cq_channel;
        //         struct ibv_cq		*recv_cq;
        //         struct ibv_srq		*srq;
        //         struct ibv_pd		*pd;
        //         enum ibv_qp_type	        qp_type;
        // };

        if(!cm_id){
		printf("(null)\n");
		return;
	}

	printf("rdma_cm_id{\n");
	if(cm_id->verbs && cm_id->verbs->device) {
                printf("\tverbs: *ibv_context{\n\t\tdevice:\n\t");
                print_ibv_device(cm_id->verbs->device);
                printf("}\n");
        }

        char rdma_ps_type_str[16] = { 0 };
        rdma_ps_str(cm_id->ps, rdma_ps_type_str);
        printf("\tps: %s\n", rdma_ps_type_str);
        printf("\tport_num: %u\n", cm_id->port_num);

        struct flag_str ibv_qp_types[] = {
                {IBV_QPT_RC, "IBV_QPT_RC"},
                {IBV_QPT_UC, "IBV_QPT_UC"},
                {IBV_QPT_UD, "IBV_QPT_UD"},
                {IBV_QPT_RAW_PACKET, "IBV_QPT_RAW_PACKET"},
                {IBV_QPT_XRC_SEND, "IBV_QPT_XRC_SEND"},
                {IBV_QPT_XRC_RECV, "IBV_QPT_XRC_RECV"},
                {IBV_QPT_DRIVER, "IBV_QPT_DRIVER"},
        };
        char ibv_qp_type_str[24] = { 0 };
        bitflags_to_str(ibv_qp_types, 7, (int)cm_id->qp_type, ibv_qp_type_str);
        printf("\tqp_type: %s\n", ibv_qp_type_str);

	printf("}\n");
}

void print_ibv_mr(const struct ibv_mr *mr)
{
        // struct ibv_mr {
        //         struct ibv_context   *context;
        //         struct ibv_pd	*pd;
        //         void		        *addr;
        //         size_t		length;
        //         uint32_t		handle;
        //         uint32_t		lkey;
        //         uint32_t		rkey;
        // };

        printf("ibv_mr{\n");
        printf("\tcontext: %p\n", mr->context);
        printf("\tpd: %p\n", mr->pd);
        printf("\taddr: %p\n", mr->addr);
        printf("\tlength: %u\n", mr->length);
        printf("\thandle: %u\n", mr->handle);
        printf("\tlkey: %u\n", mr->lkey);
        printf("\trkey: %u\n", mr->rkey);
        printf("}\n");
}

void print_ibv_sge(const struct ibv_sge *sge)
{
        // struct ibv_sge {
        //         uint64_t	addr;
        //         uint32_t	length;
        //         uint32_t	lkey;
        // };

        printf("ibv_sge{\n");
        printf("\taddr: %u\n", sge->addr);
        printf("\tlength: %u\n", sge->length);
        printf("\tlkey: %u\n", sge->lkey);
        printf("}\n");
}

void print_rdma_buffer_attr(const struct rdma_buffer_attr *rba)
{
        // struct rdma_buffer_attr {
        //         uint64_t address;
        //         uint32_t length;
        //         union stag {
        //                 uint32_t local_stag;
        //                 uint32_t remote_stag;
        //         } stag;
        // };

        printf("rdma_buffer_attr{\n");
        printf("\taddress: %u\n", rba->address);
        printf("\tlength: %u\n", rba->length);
        printf("\tstag: %u\n", rba->stag.local_stag);
        printf("}\n");
}