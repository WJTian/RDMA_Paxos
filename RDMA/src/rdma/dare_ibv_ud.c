#include "../include/rdma/dare_ibv_ud.h"
#include "../include/rdma/dare_ibv.h"
#include "../include/rdma/dare_ibv_rc.h"
#include "../include/rdma/dare_server.h"
#include "../include/util/common-structure.h"

extern dare_ib_device_t *dare_ib_device;
#define IBDEV dare_ib_device
#define SRV_DATA ((dare_server_data_t*)dare_ib_device->udata)

#define PORT_NUM 4444
#define GROUP_ADDR "225.1.1.1"
#define BUFSIZE 256

/* ================================================================== */

static reconf_rep_t* 
handle_server_join_request();
static rc_syn_t*  
handle_rc_syn(rc_syn_t *msg);
static int 
handle_rc_synack(rc_syn_t *msg);
static void 
handle_server_join_reply(reconf_rep_t *reply);
static int 
handle_server_destroy_request(rc_destroy_t *msg);
static int
ud_exchange_rc_info();

static int 
mcast_send_message(void* data, size_t len);


/* ================================================================== */

void* event(void* arg)
{
    struct ip_mreq mreq;
    struct sockaddr_in client_addr;

    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    mreq.imr_multiaddr.s_addr = inet_addr(GROUP_ADDR);
    mreq.imr_interface.s_addr = INADDR_ANY;
    
    client_addr.sin_family = AF_INET;
    client_addr.sin_addr.s_addr = INADDR_ANY;
    client_addr.sin_port = htons(PORT_NUM);
    
    int opt_on = 1;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_on, sizeof(opt_on));

    if (bind(sockfd, (struct sockaddr *)&client_addr, sizeof(struct sockaddr)) < 0)
        perror ("ERROR on binding");

    if (setsockopt(sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char*)&mreq, sizeof(mreq)) < 0)
        perror ("ERROR on setsockopt");

    socklen_t addr_len = sizeof(client_addr);

    for (;;)
    {
        void *buffer = malloc(BUFSIZE);
        original_recvfrom(sockfd, buffer, BUFSIZE ,0, (struct sockaddr*)&client_addr, &addr_len);

        if (memcpy(&client_addr.sin_addr, &SRV_DATA->my_address->sin_addr, sizeof(client_addr.sin_addr)) == 0)
            continue;

        ud_hdr_t *header = (ud_hdr_t*)buffer;
        uint32_t type = header->type;
        switch(type) {
            case JOIN:
            {
                if (!is_leader())
                    continue;
                else
                {
                    reconf_rep_t *psm_reply = handle_server_join_request(buffer);
                    original_sendto(sockfd, (void*)psm_reply, sizeof(reconf_rep_t), 0, (struct sockaddr*)&client_addr, addr_len);
                    free(psm_reply);       
                }
            }
            case RC_SYN:
            {
                rc_syn_t *reply = handle_rc_syn(buffer);
                original_sendto(sockfd, (void*)reply, reply->hdr.length, 0, (struct sockaddr*)&client_addr, addr_len);
                free(reply);
            }
            case RC_SYNACK:
            {
                handle_rc_synack(buffer);
            }
            case CFG_REPLY:
            {
                handle_server_join_reply(buffer);
                ud_exchange_rc_info();
            }
            case DESTROY:
            {
                handle_server_destroy_request(buffer);
            }
            default:
            {
            }
        }
    }
}


void ud_get_message()
{
    pthread_t cm_thread;
    pthread_create(&cm_thread, NULL, &event, NULL);
}


static int 
mcast_send_message(void* data, size_t len)
{
    struct sockaddr_in addr_dest;
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    addr_dest.sin_family = AF_INET;
    addr_dest.sin_addr.s_addr = inet_addr(GROUP_ADDR);
    addr_dest.sin_port = htons(PORT_NUM);

    socklen_t addr_len = sizeof(addr_dest);

    if (sockfd < 0)
        fprintf(stderr, "ERROR opening socket\n");

    original_sendto(sockfd, data, len, 0, (struct sockaddr*)&addr_dest, addr_len);

    if (original_close(sockfd))
        fprintf(stderr, "failed to close socket\n");

    return 0;
}

int ud_join_cluster()
{
    size_t len = sizeof(ud_hdr_t);
    ud_hdr_t *request = (ud_hdr_t*)malloc(len);
    
    memset(request, 0, len);
    request->type = JOIN;

    mcast_send_message(request, len);
    free(request);

    return 0;
}

static reconf_rep_t* 
handle_server_join_request()
{
    reconf_rep_t *psm_reply = (reconf_rep_t*)malloc(sizeof(reconf_rep_t));
    psm_reply->hdr.type = CFG_REPLY;
    psm_reply->idx = *SRV_DATA->config.idx;

    return psm_reply;
}

static int handle_server_destroy_request(rc_destroy_t *msg)
{
    uint32_t idx = msg->idx;
    dare_ib_ep_t* ep = (dare_ib_ep_t*)SRV_DATA->config.servers[idx].ep;
    ep->rc_connected = 0;

    return 0;
}


static int ud_exchange_rc_info()
{
    rc_syn_t *request = (rc_syn_t*)malloc(BUFSIZE);

    uint32_t *qpns = (uint32_t*)request->data;
    size_t len = sizeof(rc_syn_t);

    request->hdr.type      = RC_SYN;
    request->log_rm.raddr  = (uintptr_t)IBDEV->lcl_mr->addr;
    request->log_rm.rkey   = IBDEV->lcl_mr->rkey;
    request->idx           = *SRV_DATA->config.idx;
    request->lid = IBDEV->lid;

    uint32_t i, j;
    dare_ib_ep_t* ep;
    for (i = 0, j = 0; i < SRV_DATA->config.cid.size; i++, j += 1) {
        ep = (dare_ib_ep_t*)SRV_DATA->config.servers[i].ep;
        qpns[j] = ep->rc_ep.rc_qp.qp->qp_num;
    }
    len += SRV_DATA->config.cid.size*sizeof(uint32_t);
    
    union ibv_gid my_gid;
    if (IBDEV->gid_idx >= 0)
    {
        int rc = ibv_query_gid(IBDEV->ib_dev_context, IBDEV->port_num, IBDEV->gid_idx, &my_gid);
        if (rc)
            fprintf(stderr, "could not get gid for port %d, index %d\n", IBDEV->port_num, IBDEV->gid_idx);
    }
    else
        memset(&my_gid, 0, sizeof my_gid);
    memcpy(request->gid, &my_gid, 16);
    
    mcast_send_message((void*)request, len);
    free(request);
    return 0;
}

static void 
handle_server_join_reply(reconf_rep_t *reply)
{
    SRV_DATA->cur_view->leader_id = reply->idx;
}

static rc_syn_t*  
handle_rc_syn(rc_syn_t *msg)
{
    int rc;
    dare_ib_ep_t *ep;
    uint32_t *qpns = (uint32_t*)msg->data;
    
    /* Verify if RC already established */
    ep = (dare_ib_ep_t*)SRV_DATA->config.servers[msg->idx].ep;
    if (0 == ep->rc_connected) {
        
        /* Set log and ctrl memory region info */
        ep->rc_ep.rmt_mr.raddr  = msg->log_rm.raddr;
        ep->rc_ep.rmt_mr.rkey   = msg->log_rm.rkey;
        
        /* Set the remote QPNs */
        ep->rc_ep.rc_qp.qpn = qpns[*SRV_DATA->config.idx];

        uint8_t rmt_gid[16];
        memcpy(rmt_gid, msg->gid, 16);

        ep->rc_connected = 1;

        rc = rc_connect_server(msg->idx, msg->lid, rmt_gid);
        if (0 != rc) {
            fprintf(stderr, "Cannot connect server (CTRL)\n");
        }
    }

    /* Send RC_SYNACK msg */  
    rc_syn_t *reply = (rc_syn_t*)malloc(BUFSIZE);
    qpns = (uint32_t*)reply->data;
    size_t len = sizeof(rc_syn_t);
    memset(reply, 0, sizeof(rc_syn_t));
    reply->hdr.type      = RC_SYNACK;
    reply->log_rm.raddr  = (uintptr_t)IBDEV->lcl_mr->addr;
    reply->log_rm.rkey   = IBDEV->lcl_mr->rkey;
    reply->idx           = *SRV_DATA->config.idx;
    qpns[0] = ep->rc_ep.rc_qp.qp->qp_num;
    len += sizeof(uint32_t);
    reply->lid = IBDEV->lid;
    reply->hdr.length = len;

    union ibv_gid my_gid;
    if (IBDEV->gid_idx >= 0)
    {
        rc = ibv_query_gid(IBDEV->ib_dev_context, IBDEV->port_num, IBDEV->gid_idx, &my_gid);
        if (rc)
            fprintf(stderr, "could not get gid for port %d, index %d\n", IBDEV->port_num, IBDEV->gid_idx);
    }
    else
        memset(&my_gid, 0, sizeof my_gid);
    memcpy(reply->gid, &my_gid, 16);

    return reply;
}

static int 
handle_rc_synack(rc_syn_t *msg)
{
    int rc;
    dare_ib_ep_t *ep;
    uint32_t *qpns = (uint32_t*)msg->data;

    /* Verify if RC already established */
    ep = (dare_ib_ep_t*)SRV_DATA->config.servers[msg->idx].ep;
    if (0 == ep->rc_connected) {

        /* Set log and ctrl memory region info */
        ep->rc_ep.rmt_mr.raddr  = msg->log_rm.raddr;
        ep->rc_ep.rmt_mr.rkey   = msg->log_rm.rkey;

        /* Set the remote QPNs */     
        ep->rc_ep.rc_qp.qpn    = qpns[0];

        uint8_t rmt_gid[16];
        memcpy(rmt_gid, msg->gid, 16);
        
        /* Mark RC established */
        ep->rc_connected = 1;

        rc = rc_connect_server(msg->idx, msg->lid, rmt_gid);
        if (0 != rc) {
            error_return(1, log_fp, "Cannot connect server (LOG)\n");
        }

    }
    
    return 0;
}

int ud_destroy_request()
{
    uint32_t len = sizeof(rc_destroy_t);
    rc_destroy_t *request = (rc_destroy_t*)malloc(len);
    
    memset(request, 0, len);
    request->hdr.type = DESTROY;
    request->idx = *SRV_DATA->config.idx;

    mcast_send_message(request, len);
    free(request);

    return 0;
}