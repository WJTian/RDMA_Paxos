#include "../include/proxy/proxy.h"
#include "../include/config-comp/config-proxy.h"
#include "../include/replica-sys/node.h"

#include "../include/rdma/dare.h"

#include <fcntl.h>
#include <netinet/tcp.h>
#include <sys/stat.h>

void proxy_on_accept(int fd, proxy_node* proxy)
{
    uint32_t leader_id = get_leader_id(proxy->con_node);
    if (proxy->node_id == leader_id)
    {
        socket_pair* new_conn = malloc(sizeof(socket_pair));
        memset(new_conn,0,sizeof(socket_pair));

        new_conn->key = fd;
        new_conn->proxy = proxy;
        MY_HASH_SET(new_conn,proxy->hash_map);

        proxy_connect_msg* co_msg = (proxy_connect_msg*)malloc(sizeof(proxy_connect_msg));
        co_msg->header.action = P_CONNECT;
        co_msg->header.connection_id = fd;
        rsm_op(proxy->con_node, PROXY_CONNECT_MSG_SIZE, co_msg, NULL);
        free(co_msg);
    }

    return;
}

void proxy_on_close(int fd, proxy_node* proxy)
{
    uint32_t leader_id = get_leader_id(proxy->con_node);
    if (proxy->node_id == leader_id)
    {
        socket_pair* ret = NULL;
        MY_HASH_GET(&fd,proxy->hash_map,ret);
        ret->key = -1;

        proxy_close_msg* cl_msg = (proxy_close_msg*)malloc(sizeof(proxy_close_msg));
        cl_msg->header.action = P_CLOSE;
        cl_msg->header.connection_id = fd;

        rsm_op(proxy->con_node, PROXY_CLOSE_MSG_SIZE, cl_msg, NULL);
        free(cl_msg);
    }

    return;
}

void proxy_on_check(int fd, const void* buf, size_t ret, proxy_node* proxy)
{
    if (proxy->check_output && listSearchKey(proxy->excluded_fd, &fd) == NULL)
    {
        pthread_mutex_lock(&output_handler.lock);
        store_output(buf, ret);
        long output_idx = output_handler.count;
        pthread_mutex_unlock(&output_handler.lock);

        uint32_t leader_id = get_leader_id(proxy->con_node);
        if (leader_id == proxy->node_id && output_idx % CHECK_PERIOD == 0)
        {
            output_peer_t output_peers[MAX_SERVER_COUNT];
            client_side_on_read(proxy, &output_idx, sizeof(long), output_peers, fd);
            if (output_idx != CHECK_PERIOD)
            {
                uint32_t group_size = get_group_size(proxy->con_node);
                do_decision(output_peers, group_size);
            }
        }

    }
}

static int SetBlocking(int fd, int blocking) {
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

static int KeepAlive(int fd) {
    int val = 1;

    if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &val, sizeof(val)) == -1)
    {
        fprintf(stderr, "setsockopt SO_KEEPALIVE: %s", strerror(errno));
    }

    return 0;
}

void client_side_on_read(proxy_node* proxy, void *buf, size_t ret, output_peer_t* output_peers, int fd){
    uint32_t leader_id = get_leader_id(proxy->con_node);
    if (listSearchKey(proxy->excluded_fd, &fd) == NULL && proxy->rsm != 0 && proxy->node_id == leader_id)
    {
        proxy_send_msg* send_msg = (proxy_send_msg*)malloc(ret + sizeof(proxy_send_msg));

        send_msg->header.action = P_SEND;
        send_msg->header.connection_id = fd;
        send_msg->data_size = ret;
        memcpy(send_msg->data,buf,ret);
        rsm_op(proxy->con_node, PROXY_SEND_MSG_SIZE(send_msg), send_msg, output_peers);
        free(send_msg);
    }
    return;
};

static void do_action_close(size_t data_size,void* data,void* arg){
    proxy_node* proxy = arg;
    proxy_close_msg* msg = data;
    socket_pair* ret = NULL;
    MY_HASH_GET(&msg->header.connection_id,proxy->hash_map,ret);
    if(NULL==ret){
        goto do_action_close_exit;
    }else{      
        if(ret->p_s!=NULL){
            ret->key = -1;
            listNode *node = listSearchKey(proxy->excluded_fd, ret->p_s);
            listDelNode(proxy->excluded_fd, node);
        }
    }
do_action_close_exit:
    return;
}

static void do_action_connect(size_t data_size,void* data,void* arg){
    
    proxy_node* proxy = arg;
    socket_pair* ret = NULL;
    proxy_msg_header* header = data;
    MY_HASH_GET(&header->connection_id,proxy->hash_map,ret);
    if(NULL==ret){
        ret = malloc(sizeof(socket_pair));
        memset(ret,0,sizeof(socket_pair));
        ret->key = header->connection_id;
        ret->proxy = proxy;
        MY_HASH_SET(ret,proxy->hash_map);
    }
    if(ret->p_s==NULL){
        int *fd = (int*)malloc(sizeof(int));
        *fd = socket(AF_INET, SOCK_STREAM, 0);
        SetBlocking(*fd, 0);
        listAddNodeTail(proxy->excluded_fd, (void*)fd);

        connect(*fd, (struct sockaddr*)&proxy->sys_addr.s_addr,proxy->sys_addr.s_sock_len);
        ret->p_s = fd;
        SYS_LOG(proxy, "Proxy sets up socket connection (id %d) with server application.\n", ret->key);

        int enable = 1;
        if(setsockopt(*fd, IPPROTO_TCP, TCP_NODELAY, (void*)&enable, sizeof(enable)) < 0)
            printf("TCP_NODELAY SETTING ERROR!\n");
        KeepAlive(*fd);
    }
    return;
}

static void do_action_send(size_t data_size,void* data,void* arg){
    proxy_node* proxy = arg;

    proxy_send_msg* msg = data;
    socket_pair* ret = NULL;
    MY_HASH_GET(&msg->header.connection_id,proxy->hash_map,ret);

    if(NULL==ret){
        goto do_action_send_exit;
    }else{
        if(NULL==ret->p_s){
            goto do_action_send_exit;
        }else{
            SYS_LOG(proxy, "Proxy sends request to the real server.\n");
            write(*ret->p_s, msg->data, msg->data_size);
        }
    }
do_action_send_exit:
    return;
}

static void update_state(size_t data_size,void* data,void* arg){
    proxy_node* proxy = arg;

    proxy_msg_header* header = data;
    FILE* output = NULL;
    if(proxy->req_log){
        output = proxy->req_log_file;
    }
    switch(header->action){
        case P_CONNECT:
            if(output!=NULL){
                fprintf(output,"Operation: Connects.\n");
            }
            do_action_connect(data_size,data,arg);
            break;
        case P_SEND:
            if(output!=NULL){
                fprintf(output,"Operation: Sends data.\n");
            }
            do_action_send(data_size,data,arg);
            break;
        case P_CLOSE:
            if(output!=NULL){
                fprintf(output,"Operation: Closes.\n");
            }
            do_action_close(data_size,data,arg);
            break;
        default:
            break;
    }
    return;
}

int comp(void *ptr, void *key)
{
    return (*(int*)ptr == *(int*)key) ? 1 : 0;
}

proxy_node* proxy_init(node_id_t node_id, const char* config_path, const char* log_path){
    
    proxy_node* proxy = (proxy_node*)malloc(sizeof(proxy_node));

    if(NULL==proxy){
        err_log("PROXY : Cannot Malloc Memory For The Proxy.\n");
        goto proxy_exit_error;
    }

    memset(proxy, 0, sizeof(proxy_node));

    proxy->node_id = node_id;

    if(proxy_read_config(proxy,config_path)){
        err_log("PROXY : Configuration File Reading Error.\n");
        goto proxy_exit_error;
    }

    int build_log_ret = 0;
    if(log_path==NULL){
        log_path = ".";
    }else{
        if((build_log_ret=mkdir(log_path,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))!=0){
            if(errno!=EEXIST){
                err_log("PROXY : Log Directory Creation Failed,No Log Will Be Recorded.\n");
            }else{
                build_log_ret = 0;
            }
        }
    }

    if(!build_log_ret){
            char* sys_log_path = (char*)malloc(sizeof(char)*strlen(log_path)+50);
            memset(sys_log_path,0,sizeof(char)*strlen(log_path)+50);
            if(NULL!=sys_log_path){
                sprintf(sys_log_path,"%s/node-%u-proxy-sys.log",log_path,proxy->node_id);
                proxy->sys_log_file = fopen(sys_log_path,"w");
                free(sys_log_path);
            }
            if(NULL==proxy->sys_log_file && (proxy->sys_log || proxy->stat_log)){
                err_log("PROXY : System Log File Cannot Be Created.\n");
            }
            char* req_log_path = (char*)malloc(sizeof(char)*strlen(log_path)+50);
            memset(req_log_path,0,sizeof(char)*strlen(log_path)+50);
            if(NULL!=req_log_path){
                sprintf(req_log_path,"%s/node-%u-proxy-req.log",log_path,proxy->node_id);
                proxy->req_log_file = fopen(req_log_path,"w");
                free(req_log_path);
            }
            if(NULL==proxy->req_log_file && proxy->req_log){
                err_log("PROXY : Client Request Log File Cannot Be Created.\n");
            }
    }

    proxy->db_ptr = initialize_db(proxy->db_name,0);

    if(proxy->db_ptr==NULL){
        err_log("PROXY : Cannot Set Up The Database.\n");
        goto proxy_exit_error;
    }

    proxy->excluded_fd = listCreate(); /* On error, NULL is returned. Otherwise the pointer to the new list. */
    proxy->excluded_fd->match = &comp;

    if (proxy->excluded_fd == NULL)
    {
        err_log("PROXY : Cannot create excluded descriptor list.\n");
        goto proxy_exit_error;
    }

    proxy->con_node = system_initialize(node_id,proxy->excluded_fd,config_path,log_path,update_state,proxy->db_ptr,proxy);

    if(NULL==proxy->con_node){
        err_log("PROXY : Cannot Initialize Consensus Component.\n");
        goto proxy_exit_error;
    }

	return proxy;

proxy_exit_error:
    if(NULL!=proxy){
        if(NULL!=proxy->con_node){
        }
        free(proxy);
    }
    return NULL;
}