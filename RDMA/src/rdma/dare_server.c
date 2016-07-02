#include "../include/rdma/dare_ibv.h"
#include "../include/rdma/dare_server.h"

FILE *log_fp;

/* server data */
dare_server_data_t data;

/* ================================================================== */

static int
init_server_data();
static void
free_server_data();

static void
polling();
static void 
poll_ud();

/* ================================================================== */

static void
init_network_cb();
static void
poll_cb();

/* ================================================================== */

int dare_server_init(dare_server_input_t *input)
{   
    int rc;
    
    /* Initialize data fields to zero */
    memset(&data, 0, sizeof(dare_server_data_t));
    
    /* Store input into server's data structure */
    data.cur_view = input->cur_view;
    
    /* Set log file handler */
    log_fp = input->log;
    
    /* Init server data */    
    rc = init_server_data();
    if (0 != rc) {
        free_server_data();
        fprintf(stderr, "Cannot init server data\n");
        return 1;
    }

    init_network_cb();

    poll_cb();

    return 0;
}

void dare_server_shutdown()
{   
    dare_ib_srv_shutdown();
    free_server_data();
    fclose(log_fp);
    exit(1);
}

int dare_rdma_shutdown()
{   
    dare_ib_srv_shutdown();
    free_server_data();
    return 0;
}

static int init_server_data()
{
    data.config.idx = data.input->server_idx;
    data.config.len = MAX_SERVER_COUNT;
    data.config.cid.size = data.input->group_size;
    data.config.servers = (server_t*)malloc(data.config.len * sizeof(server_t));
    if (NULL == data.config.servers) {
        error_return(1, log_fp, "Cannot allocate configuration array\n");
    }
    memset(data.config.servers, 0, data.config.len * sizeof(server_t));

    /* Set up log */
    data.log = log_new();
    if (NULL == data.log) {
        error_return(1, log_fp, "Cannot allocate log\n");
    }
    
    return 0;
}

static void free_server_data()
{   
    log_free(data.log);
    
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
    rc = dare_init_ib_device();
    if (0 != rc) {
        rdma_error(log_fp, "Cannot init IB device\n");
        goto shutdown;
    }
    
    /* Init some IB data for the server */
    rc = dare_init_ib_srv_data(&data);
    if (0 != rc) {
        rdma_error(log_fp, "Cannot init IB SRV data\n");
        goto shutdown;
    }
    
    /* Init IB RC */
    rc = dare_init_ib_rc();
        if (0 != rc) {
        rdma_error(log_fp, "Cannot init IB RC\n");
        goto shutdown;
    }
    
    return;
    
shutdown:
    dare_server_shutdown();
}

/* ================================================================== */

static void poll_cb()
{
    polling();  
}

static void polling()
{
    poll_ud();
}

static void poll_ud()
{
    dare_ib_poll_ud_queue();
}

/* ================================================================== */

int is_leader() 
{
    return *data.config.idx == data.cur_view->leader_id ? 1 : 0;
}