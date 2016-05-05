#include "../include/util/common-header.h"
#include "../include/replica-sys/node.h"
#include "../include/config-comp/config-comp.h"

#include "../include/rdma/dare_ibv.h"
#include "../include/rdma/dare_server.h"
#include <zookeeper.h>

#include <sys/stat.h>

#define SRV_DATA ((dare_server_data_t*)dare_ib_device->udata)

#define ZDATALEN 1024 * 10

#define UNKNOWN_LEADER 9999

static zhandle_t *zh;
static int is_connected;

typedef int (*compfn)(const void*, const void*);

struct znode
{
    uint32_t node_id;
    char node_path[64];
};

int compare_path(struct znode *, struct znode *);

static void get_node_path(const char *buf, char *node)
{
    const char *p = buf;
    int i;
    for (i = strlen(buf) - 1; i >= 0; i--) {
        if (*(p + i) == '/') {
            break;
        }
    }
    strcpy(node, "/election/");
    strcat(node, p + i + 1);
    return;
}

void zookeeper_init_watcher(zhandle_t *izh, int type, int state, const char *path, void *context)
{
    if (type == ZOO_SESSION_EVENT)
    {
        if (state == ZOO_CONNECTED_STATE)
        {
            is_connected = 1;
        } else if (state == ZOO_EXPIRED_SESSION_STATE) {
            is_connected = 0;
            zookeeper_close(izh);
        }
    }
}

static int check_leader(view* cur_view)
{
    int rc, i, flag, zoo_data_len = ZDATALEN;

    struct String_vector *children_list = (struct String_vector *)malloc(sizeof(struct String_vector));
    rc = zoo_get_children(zh, "/election", 0, children_list);
    if (rc)
    {
        fprintf(stderr, "Error %d for zoo_get_children\n", rc);
    }
    struct znode znodes[MAX_SERVER_COUNT];

    for (i = 0; i < children_list->count; ++i)
    {
        char *zdata_buf = malloc(ZDATALEN * sizeof(char));

        char node_path[64];
        get_node_path(children_list->data[i], node_path);

        rc = zoo_get(zh, node_path, 0, zdata_buf, &zoo_data_len, NULL);
        if (rc)
        {
            fprintf(stderr, "Error %d for zoo_get\n", rc);
        }

        znodes[i].node_id = atoi(zdata_buf);
        strcpy(znodes[i].node_path, node_path);

        free(zdata_buf);
    }

    uint32_t j;
    dare_ib_ep_t *ep;
    for (j = 0; j < MAX_SERVER_COUNT; j++)
    {
        flag = 0;
        ep = (dare_ib_ep_t*)SRV_DATA->config.servers[j].ep;
        for (i = 0; i < children_list->count; i++)
        {
            if (j == znodes[i].node_id)
                flag = 1;
        }

        if (flag == 1)
            ep->rc_connected = 1;
        else
            ep->rc_connected = 0;
    }

    qsort((void*)&znodes, children_list->count, sizeof(struct znode), (compfn)compare_path);

    cur_view->leader_id = znodes[0].node_id;

    free(children_list);
    return 0;
}

int compare_path(struct znode *elem1, struct znode *elem2)
{
    return strcmp(elem1->node_path, elem2->node_path);
}

void zoo_wget_children_watcher(zhandle_t *wzh, int type, int state, const char *path, void *watcherCtx) {
    if (type == ZOO_CHILD_EVENT)
    {
        int rc = zoo_wget_children(wzh, "/election", zoo_wget_children_watcher, watcherCtx, NULL);
        if (rc)
        {
            fprintf(stderr, "Error %d for zoo_wget_children\n", rc);
        }
        check_leader((view*)watcherCtx);
    }
}

int start_zookeeper(view* cur_view, int *zoo_fd, int zoo_port, node_id_t* node_id)
{
	int rc;
	char zoo_host_port[32];
	sprintf(zoo_host_port, "localhost:%d", zoo_port);
	zh = zookeeper_init(zoo_host_port, zookeeper_init_watcher, 15000, 0, 0, 0);

    while(is_connected != 1);
    int interest, fd;
    struct timeval tv;
    zookeeper_interest(zh, &fd, &interest, &tv);
    *zoo_fd = fd;

    char str[6];
    sprintf(str, "%"PRIu32"", *node_id);
    rc = zoo_create(zh, "/election/guid-n_", str, strlen(str), &ZOO_OPEN_ACL_UNSAFE, ZOO_SEQUENCE|ZOO_EPHEMERAL, NULL, 0);
    if (rc)
        fprintf(stderr, "Error %d for zoo_create\n", rc);

    int i, zoo_data_len = ZDATALEN;
    struct String_vector *children_list = (struct String_vector *)malloc(sizeof(struct String_vector));
    rc = zoo_get_children(zh, "/election", 0, children_list);
    if (rc)
        fprintf(stderr, "Error %d for zoo_get_children\n", rc);
    struct znode znodes[MAX_SERVER_COUNT];
    for (i = 0; i < children_list->count; ++i)
    {
        char *zdata_buf = malloc(ZDATALEN * sizeof(char));

        char node_path[64];
        get_node_path(children_list->data[i], node_path);

        rc = zoo_get(zh, node_path, 0, zdata_buf, &zoo_data_len, NULL);
        if (rc)
            fprintf(stderr, "Error %d for zoo_get\n", rc);

        znodes[i].node_id = atoi(zdata_buf);
        strcpy(znodes[i].node_path, node_path);

        free(zdata_buf);
    }
    qsort((void*)&znodes, children_list->count, sizeof(struct znode), (compfn)compare_path);
    cur_view->leader_id = znodes[0].node_id;
    free(children_list);

    rc = zoo_wget_children(zh, "/election", zoo_wget_children_watcher, (void*)cur_view, NULL);
    if (rc)
    {
        fprintf(stderr, "Error %d for zoo_wget_children\n", rc);
    }
    return 0;
}

int disconnect_zookeeper()
{
    int rc = -1;
    rc = zookeeper_close(zh);
    if (rc == ZOK)
        rc = 0;
    is_connected = 0;
    return rc;
}

void launch_zoo(node* my_node, list* excluded_fds)
{
    int *zoo_fd = (int*)malloc(sizeof(int));

	my_node->cur_view.view_id = 1;
    my_node->cur_view.req_id = 0;
    my_node->cur_view.leader_id = UNKNOWN_LEADER;

    start_zookeeper(&my_node->cur_view, zoo_fd, my_node->zoo_port, my_node->node_id);
    listAddNodeTail(excluded_fds, (void*)zoo_fd);
}

int launch_rdma(node* my_node)
{
    int rc = 0;
    dare_server_input_t input = {
        .log = stdout,
        .peer_pool = my_node->peer_pool,
        .group_size = my_node->group_size,
        .server_idx = my_node->node_id,
        .cur_view = &my_node->cur_view
    };

    if (0 != dare_server_init(&input)) {
        err_log("CONSENSUS MODULE : Cannot init dare\n");
        rc = 1;
    }
    return rc;
}

int launch_replica_thread(node* my_node, list* excluded_threads)
{
    int rc = 0;
    if (pthread_create(&my_node->rep_thread,NULL,handle_accept_req,my_node->consensus_comp) != 0)
        rc = 1;
    pthread_t *replica_thread = (pthread_t*)malloc(sizeof(pthread_t));
    *replica_thread = my_node->rep_thread;
    listAddNodeTail(excluded_threads, (void*)replica_thread);
    return rc;
}

int initialize_node(node* my_node, const char* log_path, void (*user_cb)(db_key_type index,void* arg), int (*up_check)(void* arg), int (*up_get)(view_stamp clt_id,void* arg), void* db_ptr, void* arg){

    int flag = 1;

    int build_log_ret = 0;
    if(log_path==NULL){
        log_path = ".";
    }else{
        if((build_log_ret=mkdir(log_path,S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH))!=0){
            if(errno!=EEXIST){
                err_log("CONSENSUS MODULE : Log Directory Creation Failed,No Log Will Be Recorded.\n");
            }else{
                build_log_ret = 0;
            }
        }
    }
    if(!build_log_ret){
            char* sys_log_path = (char*)malloc(sizeof(char)*strlen(log_path)+50);
            memset(sys_log_path,0,sizeof(char)*strlen(log_path)+50);
            if(NULL!=sys_log_path){
                sprintf(sys_log_path,"%s/node-%u-consensus-sys.log",log_path,*my_node->node_id);
                my_node->sys_log_file = fopen(sys_log_path,"w");
                free(sys_log_path);
            }
            if(NULL==my_node->sys_log_file && (my_node->sys_log || my_node->stat_log)){
                err_log("CONSENSUS MODULE : System Log File Cannot Be Created.\n");
            }
    }

    my_node->consensus_comp = NULL;

    my_node->consensus_comp = init_consensus_comp(my_node,
            my_node->node_id,my_node->sys_log_file,my_node->sys_log,
            my_node->stat_log,my_node->db_name,db_ptr,my_node->group_size,
            &my_node->cur_view,&my_node->highest_to_commit,&my_node->highest_committed,
            &my_node->highest_seen,user_cb,up_check,up_get,arg);
    if(NULL==my_node->consensus_comp){
        goto initialize_node_exit;
    }
    
    flag = 0;
initialize_node_exit:

    return flag;
}

dare_log_entry_t* rsm_op(node* my_node, size_t ret, void *buf, uint8_t type, view_stamp* clt_id)
{
    return leader_handle_submit_req(my_node->consensus_comp,ret,buf,type,clt_id);
}

uint32_t get_leader_id(node* my_node)
{
    return my_node->cur_view.leader_id;
}

uint32_t get_group_size(node* my_node)
{
    return my_node->group_size;
}

node* system_initialize(node_id_t* node_id,const char* config_path, const char* log_path, void(*user_cb)(db_key_type index,void* arg), int(*up_check)(void* arg), int(*up_get)(view_stamp clt_id, void*arg), void* db_ptr,void* arg){

    node* my_node = (node*)malloc(sizeof(node));
    memset(my_node,0,sizeof(node));
    if(NULL==my_node){
        goto exit_error;
    }

    my_node->node_id = node_id;
    my_node->db_ptr = db_ptr;

    if(consensus_read_config(my_node,config_path)){
        err_log("CONSENSUS MODULE : Configuration File Reading Failed.\n");
        goto exit_error;
    }


    if(initialize_node(my_node,log_path,user_cb,up_check,up_get,db_ptr,arg)){
        err_log("CONSENSUS MODULE : Network Layer Initialization Failed.\n");
        goto exit_error;
    }

    return my_node;

exit_error:
    if(NULL!=my_node){
    }

    return NULL;
}
