#include "../include/ev_mgr/ev_mgr.h"
#include "../include/config-comp/config-mgr.h"
#include "../include/replica-sys/node.h"
#include "../include/rdma/dare.h"
#include "../include/ev_mgr/check_point_thread.h"

#include "../include/output/output.h"

#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/stat.h>

volatile int g_checkpoint_flag = NO_DISCONNECTED;

static pthread_t check_point_thread; // a global pid

int comp(void *ptr, void *key)
{
    return (*(int*)ptr == *(int*)key) ? 1 : 0;
}

int mgr_on_process_init(event_manager* ev_mgr)
{
	if (ev_mgr->excluded_fd != NULL)
		listRelease(ev_mgr->excluded_fd);

	ev_mgr->excluded_fd = NULL;
	ev_mgr->excluded_fd = listCreate();
	ev_mgr->excluded_fd->match = &comp;
	int rc = launch_replica_thread(ev_mgr->con_node, ev_mgr->excluded_fd);
	if (rc != 0 )
		fprintf(stderr, "EVENT MANAGER : Cannot create replica thread\n");
	if (pthread_create(&check_point_thread, NULL, &check_point_thread_start, NULL) != 0) 
		fprintf(stderr, "EVENT MANAGER : Cannot create check point thread\n");
	return rc;
}

void leader_on_accept(int fd, event_manager* ev_mgr)
{
    if (pthread_self() == check_point_thread)
        return;

    uint32_t leader_id = get_leader_id(ev_mgr->con_node);
    if (ev_mgr->node_id == leader_id)
    {
        leader_socket_pair* new_conn = malloc(sizeof(leader_socket_pair));
        memset(new_conn,0,sizeof(leader_socket_pair));

        new_conn->key = fd;
        HASH_ADD_INT(ev_mgr->leader_hash_map, key, new_conn);

        rsm_op(ev_mgr->con_node, 0, NULL, P_CONNECT, &new_conn->vs);
    }

    return;
}

int *replica_on_accept(event_manager* ev_mgr)
{
    if (pthread_self() == check_point_thread)
    {
	//fprintf(stderr, "libevent thread is called, self id is %d\n", pthread_self());
        return NULL;
    }
    //fprintf(stderr, "other thread is called, self id is %d\n", pthread_self());
    
    uint32_t leader_id = get_leader_id(ev_mgr->con_node);
    if (ev_mgr->node_id != leader_id)
    {
	request_record* retrieve_data = NULL;
        size_t data_size;
	
        while (retrieve_data == NULL){
            retrieve_record(ev_mgr->db_ptr, sizeof(db_key_type), &ev_mgr->cur_rec, &data_size, (void**)&retrieve_data);
        }
	replica_socket_pair* ret = NULL;
        HASH_FIND(hh, ev_mgr->replica_hash_map, &retrieve_data->clt_id, sizeof(view_stamp), ret);
        ret->accepted = 1;
        return &ret->s_p;
    }

    return NULL;
}

void mgr_on_close(int fd, event_manager* ev_mgr)
{
    if (pthread_self() == check_point_thread)
        return;
    
    uint32_t leader_id = get_leader_id(ev_mgr->con_node);
    if (ev_mgr->node_id == leader_id)
    {
        leader_socket_pair* ret = NULL;
        HASH_FIND_INT(ev_mgr->leader_hash_map, &fd, ret);
        if (ret == NULL)
            goto mgr_on_close_exit;

        view_stamp close_vs = ret->vs;
        HASH_DEL(ev_mgr->leader_hash_map, ret);

        rsm_op(ev_mgr->con_node, 0, NULL, P_CLOSE, &close_vs);
	// nop is only for sending the close() consensus result to the replicas.
	rsm_op(ev_mgr->con_node, 0, NULL, P_NOP, NULL);
    }
mgr_on_close_exit:
    return;
}

void mgr_on_check(int fd, const void* buf, size_t ret, event_manager* ev_mgr)
{
    if (pthread_self() == check_point_thread)
        return;
    
    if (ev_mgr->check_output && listSearchKey(ev_mgr->excluded_fd, (void*)&fd) == NULL)
    {
        store_output(fd, buf, ret);
        uint32_t leader_id = get_leader_id(ev_mgr->con_node);
        if (leader_id == ev_mgr->node_id)
        {
            long hash_index = determine_output(fd); 
            // -1 means nop
            if (hash_index == -1)
            {
                /* nothing to do */
            } else{
                leader_socket_pair* socket_pair = NULL;
                HASH_FIND_INT(ev_mgr->leader_hash_map, &fd, socket_pair);
                const dare_log_entry_t *p = rsm_op(ev_mgr->con_node, sizeof(long), &hash_index, P_OUTPUT, &socket_pair->vs);
                // how to get hash value form log_entry from p;
                //do_decision(p);// best effort
            }
        }
    }
}

static int set_blocking(int fd, int blocking) {
    int flags;

    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        fprintf(stderr, "fcntl(F_GETFL): %s", strerror(errno));
    }

    if (blocking)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;

    if (fcntl(fd, F_SETFL, flags) == -1) {
        fprintf(stderr, "fcntl(F_SETFL,O_NONBLOCK): %s", strerror(errno));
    }
    return 0;
}

static int keep_alive(int fd) {
    int val = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) == -1)
    {
        fprintf(stderr, "setsockopt SO_KEEPALIVE: %s", strerror(errno));
    }

    return 0;
}

void server_side_on_read(event_manager* ev_mgr, void *buf, size_t ret, int fd){
    if (pthread_self() == check_point_thread)
        return;
    
    uint32_t leader_id = get_leader_id(ev_mgr->con_node);
    if (ev_mgr->node_id == leader_id)
    {
        struct stat sb;
        fstat(fd, &sb);
        if ((sb.st_mode & S_IFMT) == S_IFSOCK && ev_mgr->rsm != 0 && listSearchKey(ev_mgr->excluded_fd, &fd) == NULL)
        {
            leader_socket_pair* socket_pair = NULL;
            HASH_FIND_INT(ev_mgr->leader_hash_map, &fd, socket_pair);
            rsm_op(ev_mgr->con_node, ret, buf, P_SEND, &socket_pair->vs);
        }
    }
    return;
};

static void do_action_close(view_stamp clt_id,void* arg){
    event_manager* ev_mgr = arg;
    replica_socket_pair* ret = NULL;
    HASH_FIND(hh, ev_mgr->replica_hash_map, &clt_id, sizeof(view_stamp), ret);
    if(NULL==ret){
        goto do_action_close_exit;
    }else{      
        if(ret->p_s!=NULL){
            if (close(*ret->p_s))
                fprintf(stderr, "failed to close socket\n");

            listNode *node = listSearchKey(ev_mgr->excluded_fd, (void*)ret->p_s);
            if (node == NULL)
                fprintf(stderr, "failed to find the matching key\n");
            listDelNode(ev_mgr->excluded_fd, node);

            HASH_DEL(ev_mgr->replica_hash_map, ret);
        }
    }
do_action_close_exit:
    return;
}

static void do_action_connect(view_stamp clt_id,void* arg){
    
    event_manager* ev_mgr = arg;
    replica_socket_pair* ret;

    HASH_FIND(hh, ev_mgr->replica_hash_map, &clt_id, sizeof(view_stamp), ret);
    if(NULL==ret){
        ret = malloc(sizeof(replica_socket_pair));
        memset(ret,0,sizeof(replica_socket_pair));
        ret->key = clt_id;
        ret->accepted = 0;
        HASH_ADD(hh, ev_mgr->replica_hash_map, key, sizeof(view_stamp), ret);
    }
    if(ret->p_s==NULL){
        int *fd = (int*)malloc(sizeof(int));
        *fd = socket(AF_INET, SOCK_STREAM, 0);

        listAddNodeTail(ev_mgr->excluded_fd, (void*)fd);

        connect(*fd, (struct sockaddr*)&ev_mgr->sys_addr.s_addr,ev_mgr->sys_addr.s_sock_len);

        ret->p_s = fd;
        SYS_LOG(ev_mgr, "EVENT MANAGER sets up socket connection with server application.\n");
        set_blocking(*fd, 0);
        
        int enable = 1;
        if(setsockopt(*fd, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable)) < 0)
            printf("TCP_NODELAY SETTING ERROR!\n");
        keep_alive(*fd);
        while (!ret->accepted);
    }
    return;
}

static void do_action_send(request_record *retrieve_data,void* arg){
    event_manager* ev_mgr = arg;
    replica_socket_pair* ret = NULL;
    HASH_FIND(hh, ev_mgr->replica_hash_map, &retrieve_data->clt_id, sizeof(view_stamp), ret);

    if(NULL==ret){
        goto do_action_send_exit;
    }else{
        if(NULL==ret->p_s){
            goto do_action_send_exit;
        }else{
            SYS_LOG(ev_mgr, "Event manager sends request to the real server.\n");
            write(*ret->p_s, retrieve_data->data, retrieve_data->data_size - 1);
        }
    }
do_action_send_exit:
    return;
}


//TODO reconnect_init

// 0 is ok
// reconnect works for cheng
int reconnect_inner(){
	return 0;
}


//TODO declear g_checkpoint_flag

// which is called by libevent
// will modify g_checkpoint_flag
// will be in the same thread with libevent
// 0 is ok
// 1 is rejected
// -1 is error
int disconnct_inner(){ 
    if (NO_DISCONNECTED == g_checkpoint_flag){ // safe to modify 
        g_checkpoint_flag = DISCONNECTED_REQUEST;
        // wait for approve
        while (DISCONNECTED_REQUEST == g_checkpoint_flag){ // until the state will be changed.
            // do thing.  
        }
        if (DISCONNECTED_APPROVE == g_checkpoint_flag){ // safe to disconnect
	    fprintf(stderr,"disconnect is approved\n");
            int ret = rc_disconnect_server();
            if (-1==ret){ // error
                return ret;
                //abort();
            }
            ret = disconnect_zookeeper();
            if (-1 == ret){
                return ret;
                // abort();
            }
            // disconnection is ok
            g_checkpoint_flag = NO_DISCONNECTED;
            return ret;
        }else if (NO_DISCONNECTED == g_checkpoint_flag){ // rejection
            // rejected
            return 1;
        }else{
            // bug
        }
    }else{

    }
    return 0;
}

static int check_point_condtion(void* arg)
{
    event_manager* ev_mgr = arg;
    int ret;
    if (g_checkpoint_flag == NO_DISCONNECTED)
    	ret = 0;
    else if (g_checkpoint_flag == DISCONNECTED_REQUEST) {
        unsigned int connection_num = HASH_COUNT(ev_mgr->replica_hash_map);
        if (connection_num == 0)
        {
	    fprintf(stderr, "flag is set to be DISCONNECTED_APPROVE\n");
            g_checkpoint_flag = DISCONNECTED_APPROVE;
            ret = 1;
        } else {
	    fprintf(stderr, "flag is set to be NO_DISCONNECTED\n");
            g_checkpoint_flag = NO_DISCONNECTED;
            ret = 0;
        }
    } else {
        // unknown
    }
    return ret;
}

static int get_mapping_fd(view_stamp clt_id, void*arg)
{
    event_manager* ev_mgr = arg;

    replica_socket_pair* ret;

    HASH_FIND(hh, ev_mgr->replica_hash_map, &clt_id, sizeof(view_stamp), ret);
    return ret->s_p;
}

static void update_state(db_key_type index,void* arg){
    event_manager* ev_mgr = arg;

    request_record* retrieve_data = NULL;
    size_t data_size;

    retrieve_record(ev_mgr->db_ptr, sizeof(index), &index, &data_size, (void**)&retrieve_data);
    ev_mgr->cur_rec = index;

    FILE* output = NULL;
    if(ev_mgr->req_log){
        output = ev_mgr->req_log_file;
    }
    switch(retrieve_data->type){
        case P_CONNECT:
            if(output!=NULL){
                fprintf(output,"Operation: Connects.\n");
            }
            do_action_connect(retrieve_data->clt_id,arg);
            break;
        case P_SEND:
            if(output!=NULL){
                fprintf(output,"Operation: Sends data.\n");
            }
            do_action_send(retrieve_data,arg);
            break;
        case P_CLOSE:
            if(output!=NULL){
                fprintf(output,"Operation: Closes.\n");
            }
            do_action_close(retrieve_data->clt_id,arg);
            break;
	case P_NOP:
	    if(output!=NULL){
                fprintf(output,"Operation: NOP.\n");
            }
	    break; // nop is only for sending the close() consensus result to the replcias
        default:
            break;
    }
    return;
}


event_manager* mgr_init(node_id_t node_id, const char* config_path, const char* log_path){
    
    event_manager* ev_mgr = (event_manager*)malloc(sizeof(event_manager));

    if(NULL==ev_mgr){
        err_log("EVENT MANAGER : Cannot Malloc Memory For The ev_mgr.\n");
        goto mgr_exit_error;
    }

    memset(ev_mgr, 0, sizeof(event_manager));

    ev_mgr->node_id = node_id;

    if(mgr_read_config(ev_mgr,config_path)){
        err_log("EVENT MANAGER : Configuration File Reading Error.\n");
        goto mgr_exit_error;
    }

    int build_log_ret = 0;
    if(log_path==NULL){
        log_path = ".";
    }else{
        if((build_log_ret=mkdir(log_path,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))!=0){
            if(errno!=EEXIST){
                err_log("EVENT MANAGER : Log Directory Creation Failed,No Log Will Be Recorded.\n");
            }else{
                build_log_ret = 0;
            }
        }
    }

    if(!build_log_ret){
            char* sys_log_path = (char*)malloc(sizeof(char)*strlen(log_path)+50);
            memset(sys_log_path,0,sizeof(char)*strlen(log_path)+50);
            if(NULL!=sys_log_path){
                sprintf(sys_log_path,"%s/node-%u-mgr-sys.log",log_path,ev_mgr->node_id);
                ev_mgr->sys_log_file = fopen(sys_log_path,"w");
                free(sys_log_path);
            }
            if(NULL==ev_mgr->sys_log_file && (ev_mgr->sys_log || ev_mgr->stat_log)){
                err_log("EVENT MANAGER : System Log File Cannot Be Created.\n");
            }
            char* req_log_path = (char*)malloc(sizeof(char)*strlen(log_path)+50);
            memset(req_log_path,0,sizeof(char)*strlen(log_path)+50);
            if(NULL!=req_log_path){
                sprintf(req_log_path,"%s/node-%u-mgr-req.log",log_path,ev_mgr->node_id);
                ev_mgr->req_log_file = fopen(req_log_path,"w");
                free(req_log_path);
            }
            if(NULL==ev_mgr->req_log_file && ev_mgr->req_log){
                err_log("EVENT MANAGER : Request Log File Cannot Be Created.\n");
            }
    }

    ev_mgr->db_ptr = initialize_db(ev_mgr->db_name,0);

    if(ev_mgr->db_ptr==NULL){
        err_log("EVENT MANAGER : Cannot Set Up The Database.\n");
        goto mgr_exit_error;
    }
    //ev_mgr->excluded_fd = listCreate(); /* On error, NULL is returned. Otherwise the pointer to the new list. */
    //ev_mgr->excluded_fd->match = &comp;

    /*if (ev_mgr->excluded_fd == NULL)
    {
        err_log("EVENT MANAGER : Cannot create excluded descriptor list.\n");
        goto mgr_exit_error;
    }*/

    ev_mgr->leader_hash_map = NULL;
    ev_mgr->replica_hash_map = NULL;

    ev_mgr->con_node = system_initialize(node_id,ev_mgr->excluded_fd,config_path,log_path,update_state,check_point_condtion,get_mapping_fd,ev_mgr->db_ptr,ev_mgr);

    if(NULL==ev_mgr->con_node){
        err_log("EVENT MANAGER : Cannot Initialize Consensus Component.\n");
        goto mgr_exit_error;
    }

    mgr_on_process_init(ev_mgr);
    /*if (pthread_create(&check_point_thread, NULL, &check_point_thread_start, NULL) != 0)
    	fprintf(stderr, "EVENT MANAGER : Cannot create check point thread\n");*/

	return ev_mgr;

mgr_exit_error:
    if(NULL!=ev_mgr){
        if(NULL!=ev_mgr->con_node){
        }
        free(ev_mgr);
    }
    return NULL;
}
