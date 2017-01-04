#include "../include/rdma/dare_server.h"
#include "../include/rdma/dare_ibv.h"
#include "../include/rdma/dare_ibv_rc.h"

extern dare_ib_device_t *dare_ib_device;
#define IBDEV dare_ib_device
#define SRV_DATA ((dare_server_data_t*)dare_ib_device->udata)

/* ================================================================== */

static int rc_prerequisite();
static int rc_memory_reg();
static void rc_memory_dereg();
static void rc_qp_destroy(dare_ib_ep_t* ep);
static void rc_cq_destroy(dare_ib_ep_t* ep);
static int rc_qp_create(dare_ib_ep_t* ep);
static int rc_cq_create(dare_ib_ep_t* ep);

static int rc_qp_init_to_rtr(dare_ib_ep_t *ep, uint16_t dlid, uint8_t *dgid);
static int rc_qp_rtr_to_rts(dare_ib_ep_t *ep);
static int rc_qp_reset_to_init( dare_ib_ep_t *ep);
static int poll_cq(int max_wc, struct ibv_cq *cq);
static int rc_qp_reset(dare_ib_ep_t *ep);

/* ================================================================== */

int rc_send_hb()
{
    int rc;
    dare_ib_ep_t *ep;
    uint8_t i, size;
    
    size = SRV_DATA->config.cid.size;
    
    /* Set offset accordingly */
    uint32_t offset = (uint32_t) (offsetof(dare_log_t, ctrl_data) + offsetof(ctrl_data_t, hb) + sizeof(uint64_t) * SRV_DATA->config.idx);
    
    /* Issue RDMA Write operations */

    for (i = 0; i < size; i++) {
        if (i == SRV_DATA->config.idx) continue;

        ep = (dare_ib_ep_t*)SRV_DATA->config.servers[i].ep;
        
        rem_mem_t rm;
        rm.raddr = ep->rc_ep.rmt_mr.raddr + offset;
        rm.rkey = ep->rc_ep.rmt_mr.rkey;
        /* server_id, buf, len, mr, opcode, rm, signaled, poll_completion */ 
        rc = post_send(i, &SRV_DATA->log->ctrl_data.sid, sizeof(uint64_t), IBDEV->lcl_mr, IBV_WR_RDMA_WRITE, &rm, 1, 0);
        if (0 != rc) {
            /* This should never happen */
            error_return(1, log_fp, "Cannot post send operation\n");
        }
    poll_cq(1, ep->rc_ep.rc_cq.cq);
    }

    return 0;
}

int rc_send_vote_request()
{
    int rc;
    uint8_t i, size = SRV_DATA->config.cid.size;
    uint8_t idx = SRV_DATA->config.idx;
    dare_ib_ep_t *ep;
    rem_mem_t rm;
    
    fprintf(stdout, "Set vote request\n");
    vote_req_t *request = &(SRV_DATA->log->ctrl_data.vote_req[idx]);
    request->sid = SRV_DATA->log->ctrl_data.sid;
    request->term = 1;
    request->index = (idx == FIXED_LEADER) ? 1 : 0;

    uint32_t offset = (uint32_t) (offsetof(dare_log_t, ctrl_data) + offsetof(ctrl_data_t, vote_req) + sizeof(vote_req_t) * idx);
    
    for (i = 0; i < size; i++) {
        if (idx == i || i == INIT_LEADER) continue;
        ep = (dare_ib_ep_t*)SRV_DATA->config.servers[i].ep;
        
        /* Set address and key of remote memory region */
        rm.raddr = ep->rc_ep.rmt_mr.raddr + offset;
        rm.rkey = ep->rc_ep.rmt_mr.rkey;

    rc = post_send(i, request, sizeof(vote_req_t), IBDEV->lcl_mr, IBV_WR_RDMA_WRITE, &rm , 1, 0);
        if (0 != rc) {
            /* This should never happen */
            error_return(1, log_fp, "Cannot post send operation\n");
        }
    poll_cq(1, ep->rc_ep.rc_cq.cq);
    }

    return 0;
}

int rc_send_vote_ack()
{
    int rc;
    dare_ib_ep_t *ep;
    rem_mem_t rm;
    uint8_t candidate = SID_GET_IDX(SRV_DATA->log->ctrl_data.sid);
    uint8_t idx = SRV_DATA->config.idx;

    ep = (dare_ib_ep_t*)SRV_DATA->config.servers[candidate].ep;          
       
    /* Set remote offset */
    uint32_t offset = (uint32_t)(offsetof(dare_log_t, ctrl_data) + offsetof(ctrl_data_t, vote_ack) + sizeof(uint64_t) * idx);
                            
    /* Set address and key of remote memory region */
    rm.raddr = ep->rc_ep.rmt_mr.raddr + offset;
    rm.rkey = ep->rc_ep.rmt_mr.rkey;

    /* server_id, buf, len, mr, opcode, rm, signaled, poll_completion */ 
    rc = post_send(candidate, &SRV_DATA->log->end, sizeof(uint64_t), IBDEV->lcl_mr, IBV_WR_RDMA_WRITE, &rm, 1, 0);

    if (0 != rc) {
        /* This should never happen */
        error_return(1, log_fp, "Cannot post send operation\n");
    }
    poll_cq(1, ep->rc_ep.rc_cq.cq);
    return 0;
}

int rc_init()
{
    int rc, i;

    rc = rc_prerequisite();
    if (0 != rc) {
        error_return(1, log_fp, "Cannot create RC prerequisite\n");
    }

    /* Register memory for RC */
    rc = rc_memory_reg();
    if (0 != rc) {
        error_return(1, log_fp, "Cannot register RC memory\n");
    }

    /* Create QPs for RC communication */
    for (i = 0; i < SRV_DATA->config.len; i++) {
        dare_ib_ep_t* ep = (dare_ib_ep_t*)SRV_DATA->config.servers[i].ep;

        rc = rc_cq_create(ep);
        if (0 != rc) {
            error_return(1, log_fp, "Cannot create CQ\n");
        }

        /* Create QPs for this endpoint */
        rc = rc_qp_create(ep);
        if (0 != rc) {
            error_return(1, log_fp, "Cannot create QP\n");
        }
    }
    
    return 0;
}

void rc_free()
{
    int i;
    if (NULL != SRV_DATA) {
        for (i = 0; i < SRV_DATA->config.len; i++) {
            dare_ib_ep_t* ep = (dare_ib_ep_t*)SRV_DATA->config.servers[i].ep;
            rc_qp_destroy(ep);      
            rc_cq_destroy(ep);   
        }
    }

    if (NULL != IBDEV->rc_pd) {
        ibv_dealloc_pd(IBDEV->rc_pd);
    }
    rc_memory_dereg();
}

static void rc_memory_dereg()
{
    int rc;
    
    if (NULL != IBDEV->lcl_mr) {
        rc = ibv_dereg_mr(IBDEV->lcl_mr);
        if (0 != rc) {
            rdma_error(log_fp, "Cannot deregister memory");
        }
        IBDEV->lcl_mr = NULL;
    }
}

static void rc_qp_destroy(dare_ib_ep_t* ep)
{
    int rc;

    if (NULL == ep) return;

    rc = ibv_destroy_qp(ep->rc_ep.rc_qp.qp);
    if (0 != rc) {
        rdma_error(log_fp, "ibv_destroy_qp failed because %s\n", strerror(rc));
    }
    ep->rc_ep.rc_qp.qp = NULL;

}

static void rc_cq_destroy(dare_ib_ep_t* ep)
{
    int rc;

    if (NULL == ep) return;

    rc = ibv_destroy_cq(ep->rc_ep.rc_cq.cq);
    if (0 != rc) {
        rdma_error(log_fp, "ibv_destroy_cq failed because %s\n", strerror(rc));
    }
    ep->rc_ep.rc_cq.cq = NULL;
}

static int 
rc_cq_create( dare_ib_ep_t* ep )
{
    ep->rc_ep.rc_cq.cq = ibv_create_cq(IBDEV->ib_dev_context, Q_DEPTH + 1, NULL, NULL, 0);
    if (NULL == ep->rc_ep.rc_cq.cq) {
        error_return(1, log_fp, "Cannot create CQ\n");
    }

    return 0;
}

static int 
rc_qp_create( dare_ib_ep_t* ep )
{
    struct ibv_qp_init_attr qp_init_attr;
    
    if (NULL == ep) return 0;

    memset(&qp_init_attr, 0, sizeof(qp_init_attr));
    qp_init_attr.qp_type = IBV_QPT_RC;
    qp_init_attr.recv_cq = ep->rc_ep.rc_cq.cq;
    qp_init_attr.send_cq = ep->rc_ep.rc_cq.cq;
    qp_init_attr.cap.max_inline_data = IBDEV->rc_max_inline_data;
    qp_init_attr.cap.max_send_sge = 1;  
    qp_init_attr.cap.max_recv_sge = 1;
    qp_init_attr.cap.max_recv_wr = 1;
    qp_init_attr.cap.max_send_wr = IBDEV->rc_max_send_wr;
    ep->rc_ep.rc_qp.qp = ibv_create_qp(IBDEV->rc_pd, &qp_init_attr);
    if (NULL == ep->rc_ep.rc_qp.qp) {
        error_return(1, log_fp, "Cannot create QP\n");
    }
    ep->rc_ep.rc_qp.send_count = 0;

    return 0;
}

static int rc_prerequisite()
{
    IBDEV->rc_max_send_wr = IBDEV->ib_dev_attr.max_qp_wr;
    info(log_fp, "# IBDEV->rc_max_send_wr = %"PRIu32"\n", IBDEV->rc_max_send_wr);
    
    IBDEV->rc_cqe = IBDEV->ib_dev_attr.max_cqe;
    info(log_fp, "# IBDEV->rc_cqe = %d\n", IBDEV->rc_cqe);
    
    /* Allocate a RC protection domain */
    IBDEV->rc_pd = ibv_alloc_pd(IBDEV->ib_dev_context);
    if (NULL == IBDEV->rc_pd) {
        error_return(1, log_fp, "Cannot create PD\n");
    }

    if (0 != find_max_inline(IBDEV->ib_dev_context, IBDEV->rc_pd, &IBDEV->rc_max_inline_data))
    {
        error_return(1, log_fp, "Cannot find max RC inline data\n");
    }
    info(log_fp, "# MAX_INLINE_DATA = %"PRIu32"\n", IBDEV->rc_max_inline_data);
    
    return 0;
}

static int rc_memory_reg()
{  
    /* Register memory for local log */    
    IBDEV->lcl_mr = ibv_reg_mr(IBDEV->rc_pd, SRV_DATA->log, sizeof(dare_log_t) + SRV_DATA->log->len, IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_LOCAL_WRITE);
    if (NULL == IBDEV->lcl_mr) {
        error_return(1, log_fp, "Cannot register memory because %s\n", strerror(errno));
    }
    
    return 0;
}

static int rc_qp_reset(dare_ib_ep_t *ep)
{
    int rc;
    struct ibv_qp_attr attr;

    memset(&attr, 0, sizeof(attr));
    attr.qp_state = IBV_QPS_RESET;
    rc = ibv_modify_qp(ep->rc_ep.rc_qp.qp, &attr, IBV_QP_STATE); 
    if (0 != rc) {
        error_return(1, log_fp, "ibv_modify_qp failed because %s\n", strerror(rc));
    }
    
    return 0;
}

static int rc_qp_reset_to_init(dare_ib_ep_t *ep)
{
    int rc;
    struct ibv_qp_attr attr;

    ep->rc_ep.rc_qp.send_count = 0;

    memset(&attr, 0, sizeof(attr));
    attr.qp_state        = IBV_QPS_INIT;
    attr.pkey_index      = 0;
    attr.port_num        = IBDEV->port_num;
    attr.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | 
    IBV_ACCESS_REMOTE_READ |
    IBV_ACCESS_REMOTE_ATOMIC |
    IBV_ACCESS_LOCAL_WRITE;

    rc = ibv_modify_qp(ep->rc_ep.rc_qp.qp, &attr, IBV_QP_STATE | IBV_QP_PKEY_INDEX | IBV_QP_PORT | IBV_QP_ACCESS_FLAGS); 
    if (0 != rc) {
        error_return(1, log_fp, "ibv_modify_qp failed because %s\n", strerror(rc));
    }
    return 0;
}

static int rc_qp_init_to_rtr(dare_ib_ep_t *ep, uint16_t dlid, uint8_t *dgid)
{
    int rc;
    struct ibv_qp_attr attr;

    memset(&attr, 0, sizeof(attr));
    attr.qp_state           = IBV_QPS_RTR;

    attr.path_mtu           = IBDEV->mtu;
    attr.max_dest_rd_atomic = IBDEV->ib_dev_attr.max_qp_rd_atom;
    attr.min_rnr_timer      = 12;
    attr.dest_qp_num        = ep->rc_ep.rc_qp.qpn;
    attr.rq_psn             = 0;

    attr.ah_attr.is_global     = 0;
    attr.ah_attr.dlid          = dlid;
    attr.ah_attr.port_num      = IBDEV->port_num;
    attr.ah_attr.sl            = 0;
    attr.ah_attr.src_path_bits = 0;

    if (IBDEV->gid_idx >= 0)
    {
        attr.ah_attr.is_global = 1;
        attr.ah_attr.port_num = 1;
        memcpy(&attr.ah_attr.grh.dgid, dgid, 16);
        attr.ah_attr.grh.flow_label = 0;
        attr.ah_attr.grh.hop_limit = 1;
        attr.ah_attr.grh.sgid_index = IBDEV->gid_idx;
        attr.ah_attr.grh.traffic_class = 0;
    }

    rc = ibv_modify_qp(ep->rc_ep.rc_qp.qp, &attr, IBV_QP_STATE | IBV_QP_PATH_MTU | IBV_QP_MAX_DEST_RD_ATOMIC | IBV_QP_MIN_RNR_TIMER | IBV_QP_RQ_PSN | IBV_QP_AV | IBV_QP_DEST_QPN);
    if (0 != rc) {
        error_return(1, log_fp, "ibv_modify_qp failed because %s\n", strerror(rc));
    }
    
    return 0;
}

static int rc_qp_rtr_to_rts(dare_ib_ep_t *ep)
{
    int rc;
    struct ibv_qp_attr attr;

    /* Move the QP into the RTS state */
    memset(&attr, 0, sizeof(attr));
    attr.qp_state       = IBV_QPS_RTS;
    //attr.timeout        = 5;    // ~ 131 us
    attr.timeout        = 1;    // ~ 8 us
    attr.retry_cnt      = 0;    // max is 7
    attr.rnr_retry      = 7;
    attr.sq_psn         = 0;
    attr.max_rd_atomic = IBDEV->ib_dev_attr.max_qp_rd_atom;

    rc = ibv_modify_qp(ep->rc_ep.rc_qp.qp, &attr, 
        IBV_QP_STATE | IBV_QP_TIMEOUT |
        IBV_QP_RETRY_CNT | IBV_QP_RNR_RETRY | 
        IBV_QP_SQ_PSN | IBV_QP_MAX_QP_RD_ATOMIC);
    if (0 != rc) {
        error_return(1, log_fp, "ibv_modify_qp failed because %s\n", strerror(rc));
    }
    return 0;
}

int rc_connect_server(uint8_t idx, uint16_t dlid, uint8_t *dgid)
{
    int rc;
    dare_ib_ep_t *ep = (dare_ib_ep_t*)SRV_DATA->config.servers[idx].ep;
    
    rc = rc_qp_reset_to_init(ep);
    if (0 != rc) {
        error_return(1, log_fp, "Cannot move QP to init state\n");
    }
    rc = rc_qp_init_to_rtr(ep, dlid, dgid);
    if (0 != rc) {
        error_return(1, log_fp, "Cannot move QP to RTR state\n");
    }
    rc = rc_qp_rtr_to_rts(ep);
    if (0 != rc) {
        error_return(1, log_fp, "Cannot move QP to RTS state\n");
    }

    return 0;
}

int post_send(uint32_t server_id, void *buf, uint32_t len, struct ibv_mr *mr, enum ibv_wr_opcode opcode, rem_mem_t *rm, int send_flags, int poll_completion)
{
    int rc;

    dare_ib_ep_t *ep;
    struct ibv_sge sg;
    struct ibv_send_wr wr;
    struct ibv_send_wr *bad_wr;

    /* Define some temporary variables */
    ep = (dare_ib_ep_t*)SRV_DATA->config.servers[server_id].ep;

    memset(&sg, 0, sizeof(sg));
    sg.addr   = (uint64_t)buf;
    sg.length = len;
    sg.lkey   = mr->lkey;

    memset(&wr, 0, sizeof(wr));
    wr.sg_list    = &sg;
    wr.num_sge    = 1;
    wr.opcode     = opcode;

    wr.send_flags = send_flags;

    if (poll_completion)
        poll_cq(1, ep->rc_ep.rc_cq.cq);

    if (IBV_WR_RDMA_WRITE == opcode) {
        if (len <= IBDEV->rc_max_inline_data) {
            wr.send_flags |= IBV_SEND_INLINE;
        }
    }   

    wr.wr.rdma.remote_addr = rm->raddr;

    wr.wr.rdma.rkey        = rm->rkey;
   
    rc = ibv_post_send(ep->rc_ep.rc_qp.qp, &wr, &bad_wr);
    if (0 != rc) {
        error_return(1, log_fp, "ibv_post_send failed because %s [%s]\n", 
            strerror(rc), rc == EINVAL ? "EINVAL" : rc == ENOMEM ? "ENOMEM" : rc == EFAULT ? "EFAULT" : "UNKNOWN");
    }
    return 0;
}

static int poll_cq(int max_wc, struct ibv_cq *cq)
{
    struct ibv_wc wc[1];
    int ret = -1, i, total_wc = 0;
    do {
        ret = ibv_poll_cq(cq, max_wc - total_wc, wc + total_wc);
        if (ret < 0)
        {
            fprintf(stderr, "Failed to poll cq for wc due to %d \n", ret);
            return ret;
        }
        total_wc += ret;
    } while(total_wc < max_wc);
    fprintf(stdout, "%d WC are completed \n", total_wc);
    for (i = 0; i < total_wc; i++)
    {
        if (wc[i].status != IBV_WC_SUCCESS)
        {
            fprintf(stderr, "Work completion (WC) has error status: %d (means: %s) at index %d\n", -wc[i].status, ibv_wc_status_str(wc[i].status), i);
            return -(wc[i].status);
        }
    }
    return total_wc;
}
