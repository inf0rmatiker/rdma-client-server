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

        char address_family_str[64] = { 0 };
        ai_family_to_str(rai->ai_family, address_family_str);
        printf("\tai_family: %s\n", address_family_str);

        char ai_qp_types_str[32] = { 0 };
        ibv_qp_type_to_str(rai->ai_qp_type, ai_qp_types_str);
        printf("\tai_qp_type: %s\n", ai_qp_types_str);

        char rdma_ps_str[64] = { 0 };
        rdma_ps_to_str(rai->ai_port_space, rdma_ps_str);
        printf("\tai_port_space: %s\n", rdma_ps_str);
        printf("\tai_src_len: %d\n", rai->ai_src_len);
        printf("\tai_dst_len: %d\n", rai->ai_dst_len);
        printf("\tai_src_addr: ");
        print_sockaddr(rai->ai_src_addr);
        printf("\tai_dst_addr: ");
        print_sockaddr(rai->ai_dst_addr);
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

        char node_type_str[32] = { 0 };
        ibv_node_type_to_str(device->node_type, node_type_str);
        printf("\tnode_type: %s\n", node_type_str);

        char transport_type_str[32] = { 0 };
        ibv_transport_type_to_str(device->transport_type, transport_type_str);
        printf("\transport_type: %s\n", transport_type_str);

        printf("\tname: %s\n", device->name);
        printf("\tdev_name: %s\n", device->dev_name);
        printf("\tdev_path: %s\n", device->dev_path);
        printf("\tibdev_path: %s\n", device->ibdev_path);
        printf("}\n");
}

void print_rdma_addr(const struct rdma_addr *addr)
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

        printf("rdma_addr{");
        print_sockaddr((const struct sockaddr *)&(addr->src_sin));
        print_sockaddr((const struct sockaddr *)&(addr->dst_sin));
        printf("\tibaddr: rdma_ib_addr{ sgid: 0x");
        for (uint8_t i = 0; i < 16; i++) {
                printf("%x", addr->addr.ibaddr.sgid.raw[i]);
        }
        printf("dgid: 0x");
        for (uint8_t i = 0; i < 16; i++) {
                printf("%x", addr->addr.ibaddr.dgid.raw[i]);
        }
        printf(" }\n");
        printf("}\n");
}

void print_rdma_route(const struct rdma_route *route)
{

        // struct rdma_route {
	//         struct rdma_addr	        addr;
	//         struct ibv_sa_path_rec	*path_rec;
	//         int			        num_paths;
        // };

        printf("rdma_route{\n");
        printf("\taddr: ");
        print_rdma_addr(&route->addr);
        printf("\tnum_paths: %d\n", route->num_paths);
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

        char rdma_ps_str[16] = { 0 };
        rdma_ps_to_str(cm_id->ps, rdma_ps_str);
        printf("\tps: %s\n", rdma_ps_str);
        printf("\tport_num: %u\n", cm_id->port_num);


        char qp_type_str[24] = { 0 };
        ibv_qp_type_to_str(cm_id->qp_type, qp_type_str);
        printf("\tqp_type: %s\n", qp_type_str);

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

void print_sockaddr(const struct sockaddr * addr)
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

        if (!addr) {
                printf("(null)\n");
                return;
        }

        if (addr->sa_family == AF_INET) {
                const struct sockaddr_in *addr_in = (const struct sockaddr_in *)addr;
                printf("sockaddr_in{ sin_family: AF_INET, sin_port: %d, sin_addr: %s }\n",
                       ntohs(addr_in->sin_port), inet_ntoa(addr_in->sin_addr));
        } else if (addr->sa_family == AF_INET6) {
                printf("sockaddr_in6{ sin_family: AF_INET6 }\n");
        } else {
                printf("Unknown sockaddr address family\n");
        }
}