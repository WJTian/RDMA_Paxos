FILE *log_fp;

/* server data */
server_data_t data;
/* ================================================================== */
/* libEV events */

/* An idle event that polls for different stuff... */
ev_idle poll_event;

/* ================================================================== */

int server_init(server_input_t *input)
{
    /* Initialize data fields to zero */
    memset(&data, 0, sizeof(server_data_t));
    
    /* Store input into server's data structure */
    data.input = input;
    
    /* Set log file handler */
    log_fp = input->log;

    init_server_data();

    init_network_cb();

    /* Init the poll event */
    ev_idle_init(&poll_event, poll_cb);
    ev_set_priority(&poll_event, EV_MAXPRI);

    struct proxy_node_t* proxy = proxy_init(data.config.idx, data.inout.input.srv_type);
    proxy_run(proxy);
}

static int init_server_data()
{
    /* Set up the configuration */
    data.config.idx = data.input->server_idx;
    data.config.len = MAX_SERVER_COUNT;

    /* Cannot have more than MAX_SERVER_COUNT servers */
    data.config.len = MAX_SERVER_COUNT;

    data.config.cid.size = data.input->group_size;

    for (i = 0; i < data.input->group_size; i++) {
        CID_SERVER_ADD(data.config.cid, i);
    }

    data.config.servers = (server_t*)malloc(data.config.len * sizeof(server_t));
    if (NULL == data.config.servers) {
        error_return(1, log_fp, "Cannot allocate configuration array\n");
    }
    memset(data.config.servers, 0, data.config.len * sizeof(server_t));

    data.endpoints = RB_ROOT;

    return 0;
}

static void free_server_data()
{
    ep_db_free(&data.endpoints);
    
    /* Free log */
    log_free(data.log);
    
    /* Free servers */
    if (NULL != data.config.servers) {
        free(data.config.servers);
        data.config.servers = NULL;
    }
}

/* ================================================================== */

static void init_network_cb()
{
    int rc;

    /* Init IB device */
    rc = init_ib_device(MAX_SERVER_COUNT);
    if (0 != rc) {
        error(log_fp, "Cannot init IB device\n");
    }

    /* Init some IB data for the server */
    rc = init_ib_srv_data(&data);
    if (0 != rc) {
        error(log_fp, "Cannot init IB SRV data\n");
    }

    /* Init IB RC */
    rc = init_ib_rc();
        if (0 != rc) {
        error(log_fp, "Cannot init IB RC\n");
    }

    /* Start IB UD */
    rc = start_ib_ud();
    if (0 != rc) {
        error(log_fp, "Cannot start IB UD\n");
    }
    
    /* Start poll event */   
    ev_idle_start(EV_A_ &poll_event);
    
    if (SRV_TYPE_JOIN == data.input->srv_type) {
        /* Server joining the cluster */
        join_cluster_cb();
    }
    else {
    }

}

/**
 * Send join requests to the cluster
 * Note: the timer is stopped when receiving a JOIN reply (poll_ud)
 */
static void join_cluster_cb()
{
    int rc;
    
    rc = ib_join_cluster();
    if (0 != rc) {
        error(log_fp, "Cannot join cluster\n");
    }
    
    /* Retransmit after retransmission period */
    //TODO
    
    return;  
}

/**
 * Exchange RC info
 * Note: the timer is stopped when establishing connections with 
 * at least half of the servers  
 */
static void exchange_rc_info()
{
    int rc;
    
    rc = ib_exchange_rc_info();
    if (0 != rc) {
        error(log_fp, "Exchanging RC info failed\n");
    }
    
    /* Retransmit after retransmission period */
    //TODO
    
    return;
}

/* ================================================================== */

static void poll_cb()
{
    polling();  
}

static void polling()
{
    /* Poll UD connection for incoming messages */
    poll_ud();
}

/**
 * Poll for UD messages
 */
static void poll_ud()
{
    uint8_t type = ib_poll_ud_queue();
    if (MSG_ERROR == type) {
        error(log_fp, "Cannot get UD message\n");
    }
    switch(type) {
        case CFG_REPLY:
        {
            info(log_fp, "I got accepted into the cluster: idx=%"PRIu8"\n", 
                data.config.idx); PRINT_CID_(data.config.cid);
            
            /* Start RC discovery */
            exchange_rc_info_cb()
            break;
        }
        case RC_SYNACK:
        case RC_ACK:
    }
}

/* replica side */
while (TRUE)
{
    log_entry_t* new_entry = log_add_new_entry(data.log);
    if (new_entry->idx != 0)
    {
        /* record the data persistently */
        uint64_t record_no = vstol(new_entry->term, new_entry->idx);
        request_record_t* record_data = (request_record_t*)malloc(new_entry->data.cmd.len + sizeof(request_record_t));
        record_data->data_size = new_entry->data.cmd.len;
        memcpy(record_data->data, new_entry->data.cmd.cmd, new_entry->data.cmd.len);
        store_record(data.db_ptr, sizeof(record_no), record_no, new_entry->data.cmd.len + sizeof(request_record_t), record_data);

        /* RDMA write result to the leader*/
        uint32_t remote_offset = (uint32_t)(offsetof(log_t, entries) + data.log->end);
        uint8_t leader = SID_GET_IDX(data.cached_sid);
        int* res;
        *res = 1;
        RDMA_write(leader, res, sizeof(int), remote_offset);

        data.log->tail = log->end;
        data.log->end += log_entry_len(new_entry);

        /* compare committed*/
        client_req_t* req_canbe_exed = NULL;
        while (new_entry->committed > data.log->write)
        {
            log_entry_t* entry_canbe_exed = (log_entry_t*)(data.log->entries + data.log->write);
            req_canbe_exed->cmd.cmd.len = entry_canbe_exed->data.cmd.len;
            memcpy(req_canbe_exed->cmd.cmd.data, entry_canbe_exed->data.cmd.cmd, entry_canbe_exed->data.cmd.len);
            data.log->write += log_entry_len(entry_canbe_exed);
            req_canbe_exed++;
        }
        //TODO: send req_canbe_exed to server application (maybe wake up another thread to do this)
    }
}