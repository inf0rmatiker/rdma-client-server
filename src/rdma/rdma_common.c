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
                fprintf(stderr, "CM event %s received with non-zero status: (%d)\n",
                        rdma_event_str((*event)->event),
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

static void rdma_ps_to_str(enum rdma_port_space value, char *res)
{
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

static void ibv_qp_type_to_str(enum ibv_qp_type value, char *res)
{
        switch (value) {
                case IBV_QPT_RC:
                        strcat(res, "IBV_QPT_RC");
                        break;
                case IBV_QPT_UC:
                        strcat(res, "IBV_QPT_UC");
                        break;
                case IBV_QPT_UD:
                        strcat(res, "IBV_QPT_UD");
                        break;
                case IBV_QPT_RAW_PACKET:
                        strcat(res, "IBV_QPT_RAW_PACKET");
                        break;
                case IBV_QPT_DRIVER:
                        strcat(res, "IBV_QPT_DRIVER");
                        break;
                case IBV_QPT_XRC_RECV:
                        strcat(res, "IBV_QPT_XRC_RECV");
                        break;
                case IBV_QPT_XRC_SEND:
                        strcat(res, "IBV_QPT_XRC_SEND");
                        break;
                default:
                        strcat(res, "Unknown");
        }
}

static void ibv_node_type_to_str(enum ibv_node_type value, char *res)
{
        switch (value) {
                case IBV_NODE_CA:
                        strcat(res, "IBV_NODE_CA");
                        break;
                case IBV_NODE_RNIC:
                        strcat(res, "IBV_NODE_RNIC");
                        break;
                case IBV_NODE_ROUTER:
                        strcat(res, "IBV_NODE_ROUTER");
                        break;
                case IBV_NODE_SWITCH:
                        strcat(res, "IBV_NODE_SWITCH");
                        break;
                case IBV_NODE_UNKNOWN:
                        strcat(res, "IBV_NODE_UNKNOWN");
                        break;
                case IBV_NODE_UNSPECIFIED:
                        strcat(res, "IBV_NODE_UNSPECIFIED");
                        break;
                case IBV_NODE_USNIC:
                        strcat(res, "IBV_NODE_USNIC");
                        break;
                case IBV_NODE_USNIC_UDP:
                        strcat(res, "IBV_NODE_USNIC_UDP");
                        break;
                default:
                        strcat(res, "Unknown");
        }
}

static void ibv_transport_type_to_str(enum ibv_node_type value, char *res)
{
        switch (value) {
                case IBV_TRANSPORT_IB:
                        strcat(res, "IBV_TRANSPORT_IB");
                        break;
                case IBV_TRANSPORT_IWARP:
                        strcat(res, "IBV_TRANSPORT_IWARP");
                        break;
                case IBV_TRANSPORT_UNKNOWN:
                        strcat(res, "IBV_TRANSPORT_UNKNOWN");
                        break;
                case IBV_TRANSPORT_UNSPECIFIED:
                        strcat(res, "IBV_TRANSPORT_UNSPECIFIED");
                        break;
                case IBV_TRANSPORT_USNIC:
                        strcat(res, "IBV_TRANSPORT_USNIC");
                        break;
                case IBV_TRANSPORT_USNIC_UDP:
                        strcat(res, "IBV_TRANSPORT_USNIC_UDP");
                        break;
                default:
                        strcat(res, "Unknown");
        }
}

static void ai_family_to_str(int ai_family, char *res)
{
        switch (ai_family) {
                case AF_INET:
                        strcat(res, "AF_INET");
                        break;
                case AF_INET6:
                        strcat(res, "AF_INET6");
                        break;
                case AF_IB:
                        strcat(res, "AF_IB");
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

void print_rdma_addrinfo(const struct rdma_addrinfo *rai, int i)
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

        char indent[i+1];
        memset(indent, '\t', i);
        indent[i] = '\0';

        if (!rai) {
                printf("%s(null)\n", indent);
                return;
        }

        printf("%srdma_addrinfo{\n", indent);
        struct flag_str ai_flags[] = {
                {RAI_PASSIVE, "RAI_PASSIVE"},
                {RAI_NUMERICHOST, "RAI_NUMERICHOST"},
                {RAI_NOROUTE, "RAI_NOROUTE"},
                {RAI_FAMILY, "RAI_FAMILY"}
        };
        char ai_flags_str[64] = { 0 };
        bitflags_to_str(ai_flags, 4, rai->ai_flags, ai_flags_str);
        printf("%s\tai_flags: %s\n", indent, ai_flags_str);

        char address_family_str[64] = { 0 };
        ai_family_to_str(rai->ai_family, address_family_str);
        printf("%s\tai_family: %s\n", indent, address_family_str);

        char ai_qp_types_str[32] = { 0 };
        ibv_qp_type_to_str(rai->ai_qp_type, ai_qp_types_str);
        printf("%s\tai_qp_type: %s\n", indent, ai_qp_types_str);

        char rdma_ps_str[64] = { 0 };
        rdma_ps_to_str(rai->ai_port_space, rdma_ps_str);
        printf("%s\tai_port_space: %s\n", indent, rdma_ps_str);
        printf("%s\tai_src_len: %d\n", indent, rai->ai_src_len);
        printf("%s\tai_dst_len: %d\n", indent, rai->ai_dst_len);
        printf("%s\t*ai_src_addr:\n", indent);
        print_sockaddr(rai->ai_src_addr, i+2);
        printf("%s\t*ai_dst_addr:\n", indent);
        print_sockaddr(rai->ai_dst_addr, i+2);
        printf("%s\t*ai_src_canonname: %s\n", indent, rai->ai_src_canonname);
        printf("%s\t*ai_dst_canonname: %s\n", indent, rai->ai_dst_canonname);
        printf("%s\tai_route_len: %d\n", indent, rai->ai_route_len);
        printf("%s\t*ai_route: %p\n", indent, rai->ai_route);
        printf("%s\tai_connect_len: %d\n", indent, rai->ai_connect_len);
        printf("%s\t*ai_connect: %p\n", indent, rai->ai_connect);
        printf("%s\t*ai_next: %p\n", indent, rai->ai_next);
        if (rai->ai_next) {
                printf("%s},\n", indent);
                print_rdma_addrinfo(rai->ai_next, i);
        } else {
                printf("%s}\n", indent);
        }
}

void print_ibv_device(const struct ibv_device *device, int i)
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

        char indent[i+1];
        memset(indent, '\t', i);
        indent[i] = '\0';

        if (!device) {
                printf("%s(null)\n", indent);
                return;
        }

        printf("%sibv_device{\n", indent);

        char node_type_str[32] = { 0 };
        ibv_node_type_to_str(device->node_type, node_type_str);
        printf("%s\tnode_type: %s\n", indent, node_type_str);

        char transport_type_str[32] = { 0 };
        ibv_transport_type_to_str(device->transport_type, transport_type_str);
        printf("%s\ttransport_type: %s\n", indent, transport_type_str);

        printf("%s\tname: %s\n", indent, device->name);
        printf("%s\tdev_name: %s\n", indent, device->dev_name);
        printf("%s\tdev_path: %s\n", indent, device->dev_path);
        printf("%s\tibdev_path: %s\n", indent, device->ibdev_path);
        printf("%s}\n", indent);
}

void print_rdma_addr(const struct rdma_addr *addr, int i)
{
        // struct rdma_addr {
        //         union {
        //                 struct sockaddr		src_addr;
        //                 struct sockaddr_in	        src_sin;
        //                 struct sockaddr_in6	        src_sin6;
        //                 struct sockaddr_storage      src_storage;
        //         };
        //         union {
        //                 struct sockaddr		dst_addr;
        //                 struct sockaddr_in	        dst_sin;
        //                 struct sockaddr_in6	        dst_sin6;
        //                 struct sockaddr_storage      dst_storage;
        //         };
        //         union {
        //                 struct rdma_ib_addr	        ibaddr;
        //         } addr;
        // };

        // struct rdma_ib_addr {
        //         union ibv_gid	sgid;
        //         union ibv_gid	dgid;
        //         __be16		pkey;
        // };

        // union ibv_gid {
        //         uint8_t		raw[16];
        //         struct {
        //                 __be64	subnet_prefix;
        //                 __be64	interface_id;
        //         } global;
        // };

        char indent[i+1];
        memset(indent, '\t', i);
        indent[i] = '\0';

        if (!addr) {
                printf("%s(null)\n", indent);
                return;
        }

        printf("%srdma_addr{\n", indent);
        printf("%s\tsrc_addr:", indent);
        print_sockaddr(&addr->src_addr, i+2);
        printf("%s\tdst_addr:", indent);
        print_sockaddr(&addr->dst_addr, i+2);
        printf("%s\tibaddr: rdma_ib_addr{ sgid: 0x", indent);
        for (uint8_t i = 10; i < 16; i++) {
                printf("%x", addr->addr.ibaddr.sgid.raw[i]);
        }
        printf(" dgid: 0x");
        for (uint8_t i = 10; i < 16; i++) {
                printf("%x", addr->addr.ibaddr.dgid.raw[i]);
        }
        printf(" }\n");
        printf("%s}\n", indent);
}

void print_rdma_route(const struct rdma_route *route, int i)
{
        // struct rdma_route {
	//         struct rdma_addr	        addr;
	//         struct ibv_sa_path_rec	*path_rec;
	//         int			        num_paths;
        // };

        char indent[i+1];
        memset(indent, '\t', i);
        indent[i] = '\0';

        if (!route) {
                printf("%s(null)\n", indent);
                return;
        }

        printf("%srdma_route{\n", indent);
        printf("%s\taddr:\n", indent);
        print_rdma_addr(&route->addr, i+2);
        printf("%s\tnum_paths: %d\n", indent, route->num_paths);
        printf("%s}\n", indent);
}

void print_rdma_cm_id(const struct rdma_cm_id *cm_id, int i)
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

        char indent[i+1];
        memset(indent, '\t', i);
        indent[i] = '\0';

        if(!cm_id){
		printf("%s(null)\n", indent);
		return;
	}

	printf("%srdma_cm_id{\n", indent);
        if (!cm_id->verbs) {
                printf("%s\t*verbs: (null)\n", indent);
        } else if (cm_id->verbs->device) {
                printf("%s\tverbs:\n", indent);
                print_ibv_context(cm_id->verbs, i+2);
        }

        char rdma_ps_str[16] = { 0 };
        rdma_ps_to_str(cm_id->ps, rdma_ps_str);
        printf("%s\tps: %s\n", indent, rdma_ps_str);
        printf("%s\tport_num: %u\n", indent, cm_id->port_num);

        char qp_type_str[24] = { 0 };
        ibv_qp_type_to_str(cm_id->qp_type, qp_type_str);
        printf("%s\tqp_type: %s\n", indent, qp_type_str);

	printf("%s}\n", indent);
}

void print_ibv_context(const struct ibv_context *context, int i)
{
        // struct ibv_context {
        //         struct ibv_device            *device;
        //         struct ibv_context_ops	ops;
        //         int			        cmd_fd;
        //         int			        async_fd;
        //         int			        num_comp_vectors;
        //         pthread_mutex_t		mutex;
        //         void		                *abi_compat;
        // };

        char indent[i+1];
        memset(indent, '\t', i);
        indent[i] = '\0';

        if (!context) {
                printf("%s(null)\n", indent);
                return;
        }

        printf("%sibv_context{\n", indent);
        if (!context->device) {
                printf("%s\t*device: (null)\n", indent);
        } else {
                printf("%s\t*device:\n", indent);
                print_ibv_device(context->device, i+2);
        }
        printf("%s\tops: ibv_context_ops{ ... }\n", indent);
        printf("%s\tcmd_fd: %d\n", indent, context->cmd_fd);
        printf("%s\tasync_fd: %d\n", indent, context->async_fd);
        printf("%s\tnum_comp_vectors: %d\n", indent, context->num_comp_vectors);
        printf("%s\tmutex: { ... }\n", indent);
        printf("%s\t*abi_compat: %p\n", indent, context->abi_compat);
        printf("%s}\n", indent);
}

void print_ibv_mr(const struct ibv_mr *mr, int i)
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

        char indent[i+1];
        memset(indent, '\t', i);
        indent[i] = '\0';

        if (!mr) {
                printf("%s(null)\n", indent);
                return;
        }

        printf("%sibv_mr{\n");
        printf("%s\tcontext: %p\n", indent, mr->context);
        printf("%s\tpd: %p\n", indent, mr->pd);
        printf("%s\taddr: %p\n", indent, mr->addr);
        printf("%s\tlength: %u\n", indent, mr->length);
        printf("%s\thandle: %u\n", indent, mr->handle);
        printf("%s\tlkey: %u\n", indent, mr->lkey);
        printf("%s\trkey: %u\n", indent, mr->rkey);
        printf("%s}\n", indent);
}

void print_ibv_sge(const struct ibv_sge *sge, int i)
{
        // struct ibv_sge {
        //         uint64_t	addr;
        //         uint32_t	length;
        //         uint32_t	lkey;
        // };

        char indent[i+1];
        memset(indent, '\t', i);
        indent[i] = '\0';

        if (!sge) {
                printf("%s(null)\n", indent);
                return;
        }

        printf("%sibv_sge{\n", indent);
        printf("%s\taddr: %u\n", indent, sge->addr);
        printf("%s\tlength: %u\n", indent, sge->length);
        printf("%s\tlkey: %u\n", indent, sge->lkey);
        printf("%s}\n", indent);
}

void print_ibv_recv_wr(const struct ibv_recv_wr *recv_wr, int i)
{
        // struct ibv_recv_wr {
        //         uint64_t		wr_id;
        //         struct ibv_recv_wr   *next;
        //         struct ibv_sge	*sg_list;
        //         int			num_sge;
        // };

        char indent[i+1];
        memset(indent, '\t', i);
        indent[i] = '\0';

        if (!recv_wr) {
                printf("%s(null)\n", indent);
                return;
        }

        printf("%sibv_recv_wr{\n", indent);
        printf("%s\twr_id: %d\n", indent, recv_wr->wr_id);
        printf("%s\t*next: %p\n", indent, recv_wr->next);
        printf("%s\t*sg_list:\n");
        print_ibv_sge(recv_wr->sg_list, i+2);
        printf("%s\tnum_sge: %d\n", indent, recv_wr->num_sge);
        if (recv_wr->next) {
                printf("%s},\n", indent);
                print_ibv_recv_wr(recv_wr->next, i);
        } else {
                printf("%s}\n", indent);
        }
}

void print_rdma_buffer_attr(const struct rdma_buffer_attr *rba, int i)
{
        // struct rdma_buffer_attr {
        //         uint64_t address;
        //         uint32_t length;
        //         union stag {
        //                 uint32_t local_stag;
        //                 uint32_t remote_stag;
        //         } stag;
        // };

        char indent[i+1];
        memset(indent, '\t', i);
        indent[i] = '\0';

        if (!rba) {
                printf("%s(null)\n", indent);
                return;
        }

        printf("%srdma_buffer_attr{\n", indent);
        printf("%s\taddress: %u\n", indent, rba->address);
        printf("%s\tlength: %u\n", indent, rba->length);
        printf("%s\tstag: %u\n", indent, rba->stag.local_stag);
        printf("%s}\n", indent);
}

void print_sockaddr(const struct sockaddr *addr, int i)
{

        // struct sockaddr {
        //    sa_family_t     sa_family;      /* Address family */
        //    char            sa_data[];      /* Socket address */
        // };

        // struct sockaddr_in {
        //    sa_family_t     sin_family;     /* AF_INET */
        //    in_port_t       sin_port;       /* Port number */
        //    struct in_addr  sin_addr;       /* IPv4 address */
        // };

        // struct in_addr {
        //    in_addr_t s_addr;
        // };

        char indent[i+1];
        memset(indent, '\t', i);
        indent[i] = '\0';

        if (!addr) {
                printf("%s(null)\n", indent);
                return;
        }

        if (addr->sa_family == AF_INET) {
                const struct sockaddr_in *addr_in = (const struct sockaddr_in *)addr;
                printf("%ssockaddr_in{ sin_family: AF_INET, sin_port: %d, sin_addr: %s }\n",
                       indent,
                       ntohs(addr_in->sin_port), inet_ntoa(addr_in->sin_addr));
        } else if (addr->sa_family == AF_INET6) {
                printf("%ssockaddr_in6{ sin_family: AF_INET6 }\n", indent);
        } else {
                printf("%sUnknown sockaddr address family\n", indent);
        }
}