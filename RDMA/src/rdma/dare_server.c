#include "../include/rdma/dare_ibv.h"
#include "../include/rdma/dare_server.h"

FILE *log_fp;

/* server data */
dare_server_data_t data;

ev_idle poll_event;
ev_timer hb_event;

double hb_period;
const uint64_t elec_timeout_low = 100000;
const uint64_t elec_timeout_high = 300000;

static double random_election_timeout();
static double hb_timeout();
static void start_election();
static void poll_vote_count();

static void polling();
static void poll_vote_requests();

static void *hb_begin(void *arg);
static void hb_receive_cb( EV_P_ ev_timer *w, int revents );
static void hb_send_cb( EV_P_ ev_timer *w, int revents );
static void poll_cb( EV_P_ ev_idle *w, int revents );

static int init_server_data();
static void free_server_data();
static void init_network_cb();

#define IS_CANDIDATE ((SID_GET_IDX(data.log->ctrl_data.sid) == data.config.idx) && (!SID_GET_L(data.log->ctrl_data.sid)))

int dare_server_init(dare_server_input_t *input)
{   
    int rc;
    
    /* Initialize data fields to zero */
    memset(&data, 0, sizeof(dare_server_data_t));
    
    /* Store input into server's data structure */
    data.input = input;

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

    if (data.input->hb_on == 1)
    {
        hb_period = data.input->hb_period;

        pthread_t hb_thread;
        rc = pthread_create(&hb_thread, NULL, hb_begin, NULL);
        if (rc != 0)
            fprintf(stderr, "pthread_create hb_begin fail\n");
    }

    return 0;
}

void dare_server_shutdown()
{   
    dare_ib_srv_shutdown();
    free_server_data();
    fclose(log_fp);
    exit(1);
}

static int init_server_data()
{
    int i;

    data.config.idx = data.input->server_idx;
    fprintf(stderr, "My nid is %d\n", data.config.idx);
    data.config.len = MAX_SERVER_COUNT;
    data.config.cid.size = data.input->group_size;
    data.config.servers = (server_t*)malloc(data.config.len * sizeof(server_t));
    if (NULL == data.config.servers) {
        error_return(1, log_fp, "Cannot allocate configuration array\n");
    }
    memset(data.config.servers, 0, data.config.len * sizeof(server_t));

    for (i = 0; i < data.config.len; i++) {
        data.config.servers[i].peer_address = data.input->peer_pool[i].peer_address;
    }

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
/* HB mechanism */

static void *hb_begin(void *arg)
{
    uint64_t sid = 0;    
    
    SID_SET_TERM(sid, 1);
    SID_SET_L(sid);
    SID_SET_IDX(sid, INIT_LEADER);
    server_update_sid(sid, data.log->ctrl_data.sid);
    data.loop = EV_DEFAULT;
    /* Init the poll event */
    ev_idle_init(&poll_event, poll_cb);
    //ev_set_priority(&poll_event, EV_MAXPRI);

    // initially, send cb is called every 0.01, receive cb is called every 0.1
    if (data.config.idx == INIT_LEADER)
    {
        ev_init(&hb_event, hb_send_cb);
        hb_event.repeat = hb_period;
    } else {
        ev_init(&hb_event, hb_receive_cb);
        hb_event.repeat = hb_timeout();
        //ev_set_priority(&hb_event, EV_MAXPRI-1);
    }
    ev_timer_again(data.loop, &hb_event);

    /* Now wait for events to arrive */
    ev_run(data.loop, 0);

    return NULL;
}

static void hb_receive_cb(EV_P_ ev_timer *w, int revents)
{
    uint64_t hb;
    int timeout = 1;
    uint8_t i, size;
    size = data.config.cid.size;
    if (data.config.idx == INIT_LEADER)
	return;
    for (i = 0; i < size; i++)
    {
        if (i== data.config.idx)
            continue;

        /* Read HB and then reset it */
        hb = data.log->ctrl_data.hb[i];
        data.log->ctrl_data.hb[i] = data.log->ctrl_data.hb[i] & 0;
        //hb = __sync_fetch_and_and(&data.log->ctrl_data.hb[i], 0); //FIXME: not support atomic operations becasue of the memalign
        if (0 == hb)
        {
        	/* No heartbeat */
        	continue;
        }

        /* Somebody sent me an up-to-date HB */

        timeout = 0;
        /* Check if it is from a leader */
        if (SID_GET_L(hb)) {
		fprintf(stdout, "Received HB: [%010"PRIu64"|%d|%03"PRIu8"]\n", SID_GET_TERM(hb), (SID_GET_L(hb) ? 1 : 0), SID_GET_IDX(hb));
        }
    }

    if (timeout) {
        w->repeat = 0.;
        ev_timer_again(EV_A_ w);
        start_election();
	/* Start poll event */
	ev_idle_start(EV_A_ &poll_event);
        return;
    }
rearm:    
    /* Rearm HB event */
    w->repeat = hb_timeout();
    ev_timer_again(EV_A_ w);
}

static double hb_timeout()
{
    return 10 * hb_period;
}

static double random_election_timeout()
{
    /* Generate time in microseconds in given interval */
    struct timeval tv;
    gettimeofday(&tv,NULL);
    uint64_t seed = data.config.idx*((tv.tv_sec%100)*1e6+tv.tv_usec);
    srand48(seed);
    uint64_t timeout = (lrand48() % (elec_timeout_high-elec_timeout_low)) + elec_timeout_low;
    return (double)timeout * 1e-6;
}

/* ================================================================== */
/* Leader election */

static void start_election()
{
    int rc, i;
    
    /* Get the latest SID */
    uint64_t new_sid = 0;    
    
    /* Set SID to [t+1|0|own_idx] */
    SID_SET_TERM(new_sid, SID_GET_TERM(data.log->ctrl_data.sid) + 1);
    SID_UNSET_L(new_sid);                   // no leader :(
    SID_SET_IDX(new_sid, data.config.idx);  // I can be the leader :)
    rc = server_update_sid(new_sid, data.log->ctrl_data.sid);
    if (0 != rc) {
        return;
    }
   
    uint8_t size = data.config.cid.size;
    for (i = 0; i < size; i++) {
        /* Clear votes from a previous election */
        data.log->ctrl_data.vote_ack[i] = data.log->len;
    }

    /* Restart HB mechanism in receive mode; cannot wait forever for 
    followers to respond to me */
    ev_set_cb(&hb_event, hb_receive_cb);
    hb_event.repeat = random_election_timeout();
    ev_timer_again(data.loop, &hb_event); 
    
    /* Send vote requests */
    rc = dare_ib_send_vote_request();
    if (0 != rc) {
        /* This should never happen */
        fprintf(stderr, "Cannot send vote request\n");
    }
}

static void hb_send_cb( EV_P_ ev_timer *w, int revents )
{
    int rc;

    /* Send HB to all servers */
    //fprintf(stderr, "Sending HB\n");
    rc = dare_ib_send_hb();
    if (0 != rc) {
        rdma_error(log_fp, "Cannot send heartbeats\n");
    }
    
    /* Rearm timer */
    w->repeat = hb_period;
    ev_timer_again(EV_A_ w);
    
    return;
}

static void poll_vote_count()
{
    uint8_t vote_count;
    vote_count = 1;
    uint8_t i, size = data.config.cid.size;
    uint64_t remote_commit;
    
    //fprintf(stdout, "Start counting votes...\n");
    for (i = 0; i < size; i++) {
        if (i == data.config.idx) continue;
        remote_commit = data.log->ctrl_data.vote_ack[i];
        if (data.log->len == remote_commit) {
            /* No reply from this server */
            continue;
        }
        /* Count votes */
	vote_count++;
    }
        
    if (vote_count <  data.config.cid.size / 2 + 1) {
        return;
    }

    fprintf(stdout, "Votes:");
    for (i = 0; i < size; i++) {
        if (i == data.config.idx) continue;
        remote_commit = data.log->ctrl_data.vote_ack[i];
        if (data.log->len != remote_commit) {
            fprintf(stdout, " (p%"PRIu8")", i);
        }
    }
    fprintf(stdout, "\n");


    /**
     *  Won election: become leader 
     */
    fprintf(stdout, "vote count = %"PRIu8"\n", vote_count);
    
    /* Update own SID to [t|1|own_idx] */
    uint64_t new_sid = data.log->ctrl_data.sid;
    SID_SET_L(new_sid);
    int rc = server_update_sid(new_sid, data.log->ctrl_data.sid);
    if (0 != rc) {
        return;
    } 

become_leader:
    /* Start sending heartbeats */
    ev_set_cb(&hb_event, hb_send_cb);
    hb_event.repeat = NOW;
    ev_timer_again(data.loop, &hb_event);
}

static void poll_vote_requests()
{
    int rc;
    uint8_t i, size = data.config.cid.size;
    uint64_t new_sid;
    vote_req_t *request;
    
    /* To avoid crazy servers removing good leaders */
    if (SID_GET_L(data.log->ctrl_data.sid)) {
        fprintf(stdout, "Active leader known; just ignore vote requests\n");
        return;
    }

    /*Note: set the L flag to avoid voting twice in the same term:
          SID=[TERM|L|IDX] => [TERM|1|voted_idx] > [TERM|0|*] */
    uint64_t old_sid = data.log->ctrl_data.sid; SID_SET_L(old_sid);
    uint64_t best_sid = old_sid;
    for (i = 0; i < size; i++) {
        if (i == data.config.idx) continue;
        request = &(data.log->ctrl_data.vote_req[i]);
        if (request->sid != 0) {
		fprintf(stdout, "Vote request from: [%010"PRIu64"|%d|%03"PRIu8"]\n", SID_GET_TERM(request->sid), (SID_GET_L(request->sid) ? 1 : 0), SID_GET_IDX(request->sid)); 
        }
        if (best_sid >= request->sid) {
	    /* Compare term */
            /* Candidate's SID is not good enough; drop request */
            request->sid = 0;
            continue;
        }
        /* Don't reset "request->sid" yet */
        best_sid = request->sid;
        /* Note: we iterate through the vote requests in idx order; thus, 
        other requests for the same term are also considered */
    }
    if (best_sid == old_sid) {
        /* No better SID */
        return;
    }
    
    /* I thought I saw a better candidate ... */
    uint64_t highest_term = SID_GET_TERM(best_sid); 
    
    /* Create not committed buffer & get best request */
    vote_req_t best_request;
    best_request.sid = old_sid;

    best_request.index = (data.config.idx == FIXED_LEADER) ? 1 : 0;
    best_request.term  = 1;
    fprintf(stdout, "   # Local [idx=%"PRIu64"; term=%"PRIu64"]\n", best_request.index, best_request.term);

    /* Choose the best candidate */
    for (i = 0; i < size; i++) {
        request = &(data.log->ctrl_data.vote_req[i]);
        if (best_request.sid > request->sid) {
            /* Candidate's SID is not good enough; drop request */
            request->sid = 0;
            continue;
        }
        if (highest_term < SID_GET_TERM(request->sid))
            highest_term = SID_GET_TERM(request->sid);
	    fprintf(stdout, "   # Remote(p%"PRIu8") [idx=%"PRIu64"; term=%"PRIu64"]\n", i, request->index, request->term);
        if ( (best_request.term > request->term) || 
             ((best_request.term == request->term) && 
              (best_request.index > request->index)) )
        {
            continue;
        }
        /* I like this candidate */
        best_request.index = request->index;
        best_request.term = request->term;
        best_request.sid = request->sid;
        request->sid = 0;
    }

    fprintf(stdout, "   Best [idx=%"PRIu64"; term=%"PRIu64"]\n", best_request.index, best_request.term);
    fprintf(stdout, "   Best [%010"PRIu64"|%d|%03"PRIu8"]\n", SID_GET_TERM(best_request.sid), (SID_GET_L(best_request.sid) ? 1 : 0), SID_GET_IDX(best_request.sid));
    fprintf(stdout, "   Old [%010"PRIu64"|%d|%03"PRIu8"]\n", SID_GET_TERM(old_sid), (SID_GET_L(old_sid) ? 1 : 0), SID_GET_IDX(old_sid));

    if (best_request.sid == old_sid) {
        /* Local log is better than remote logs; yet, local term is too low. 
        Increase TERM to increase chances to win election */
        new_sid = data.log->ctrl_data.sid;
        SID_SET_TERM(new_sid, highest_term);
        SID_SET_IDX(new_sid, data.config.idx);  // don't vote for anyone
        rc = server_update_sid(new_sid, data.log->ctrl_data.sid);
        if (0 != rc) {
            /* Could not update my SID; just return */
            return;
        }
        return;
    }
     
    /* Stop HB mechanism for the moment ... */
    hb_event.repeat = 0.;
    ev_timer_again(data.loop, &hb_event); 
    
    /* Update my local SID to show support */
    rc = server_update_sid(best_request.sid, data.log->ctrl_data.sid); // FIXME
    if (0 != rc) {
        /* Could not update my SID; just return */
        return;
    }
    
    /* Send ACK to the candidate */
    rc = dare_ib_send_vote_ack();
    if (rc < 0) {
        /* Operation failed; start an election */
        start_election(); 
        return;
    }
    if (0 != rc) {
        /* This should never happen */
        fprintf(stderr, "Cannot send vote ack\n");
    }
    //debug(log_fp, "Send vote ACK\n");

    /* Restart HB mechanism in receive mode */
    ev_set_cb(&hb_event, hb_receive_cb);
    double tmp = hb_timeout();
    hb_event.repeat = tmp;
    ev_timer_again(data.loop, &hb_event); 
}

static void polling()
{   
    if (IS_CANDIDATE) {
        poll_vote_count();
    
    }
    poll_vote_requests();
}

static void poll_cb( EV_P_ ev_idle *w, int revents )
{
    polling();  
}

int server_update_sid( uint64_t new_sid, uint64_t old_sid )
{
    //int rc = __sync_bool_compare_and_swap(&(data.log->ctrl_data.sid), old_sid, new_sid); //FIXME
    if (data.log->ctrl_data.sid == old_sid)
    	data.log->ctrl_data.sid = new_sid;
    return 0;
}

