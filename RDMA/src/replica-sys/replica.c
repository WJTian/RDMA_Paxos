#include "../include/util/common-header.h"
#include "../include/replica-sys/node.h"
#include "../include/config-comp/config-comp.h"

#include "../include/rdma/dare_ibv.h"
#include "../include/rdma/dare_server.h"
#include <zookeeper.h>

#include <sys/stat.h>

#define SRV_DATA ((dare_server_data_t*)dare_ib_device->udata)

#define ZDATALEN 1024 * 10

static zhandle_t *zh;
static int is_connected;
static int zoo_port;
static node_id_t myid;

typedef int (*compfn)(const void*, const void*);

struct znodes_data
{
    uint32_t node_id;
    uint64_t tail;
    char znode_path[64];
};

int compare_tail(struct znodes_data *, struct znodes_data *);
int compare_path(struct znodes_data *, struct znodes_data *);

struct watcherContext
{
    pthread_mutex_t *lock;
    char znode_path[64];
    view* cur_view;
};

static void get_znode_path(const char *pathbuf, char *znode_path)
{
    const char *p = pathbuf;
    int i;
    for (i = strlen(pathbuf); i >= 0; i--)
    {
        if (*(p + i) == '/')
        {
            break;
        }
    }
    strcpy(znode_path, "/election/");
    strcat(znode_path, p + i + 1);
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

static int check_leader(view* cur_view, char *znode_path)
{
    int rc, i, zoo_data_len = ZDATALEN;
    char str[64];

    sprintf(str, "%"PRIu32",%"PRIu64"", myid, SRV_DATA->log->tail);
    rc = zoo_set(zh, znode_path, str, strlen(str), -1);
    if (rc)
    {
        fprintf(stderr, "Error %d for zoo_set\n", rc);
    }
    struct String_vector *children_list = (struct String_vector *)malloc(sizeof(struct String_vector));
    rc = zoo_get_children(zh, "/election", 0, children_list);
    if (rc)
    {
        fprintf(stderr, "Error %d for zoo_get_children\n", rc);
    }
    char *p;
    struct znodes_data znodes[MAX_SERVER_COUNT];

    for (i = 0; i < children_list->count; ++i)
    {
        char *zoo_data = malloc(ZDATALEN * sizeof(char));
        char zpath[64];
        get_znode_path(children_list->data[i], zpath);
        rc = zoo_get(zh, zpath, 0, zoo_data, &zoo_data_len, NULL);
        if (rc)
        {
            fprintf(stderr, "Error %d for zoo_get\n", rc);
        }
        if (*zoo_data == 'n')
        {
            znodes[i].node_id = 9999;
            znodes[i].tail = SRV_DATA->log->len;
        } else{
            p = strtok(zoo_data, ",");
            znodes[i].node_id = atoi(p);
            p = strtok(NULL, ",");
            znodes[i].tail = atoi(p);
        }
        strcpy(znodes[i].znode_path, zpath);
        free(zoo_data);
    }
    qsort((void*)&znodes, children_list->count, sizeof(struct znodes_data), (compfn)compare_tail);

    for (i = 1; i < children_list->count; i++)
    {
        if (znodes[i].tail != znodes[0].tail)
            continue;
    }
    int num_max_tail = i;
    qsort((void*)&znodes, num_max_tail, sizeof(struct znodes_data), (compfn)compare_path);    

    cur_view->leader_id = znodes[0].node_id;

    if (cur_view->leader_id == myid)
    {
        fprintf(stderr, "I am the leader\n");
        //recheck
    }else{
        fprintf(stderr, "I am a follower\n");
        // RDMA read
        // update view
        // zoo_set
    }
    free(children_list);
    return 0;
}

int compare_tail(struct znodes_data *elem1, struct znodes_data *elem2)
{
    return ((elem2->tail > elem1->tail) ? 1 : 0);
}

int compare_path(struct znodes_data *elem1, struct znodes_data *elem2)
{
    return strcmp(elem1->znode_path, elem2->znode_path);
}

void zoo_wget_children_watcher(zhandle_t *wzh, int type, int state, const char *path, void *watcherCtx) {
    if (type == ZOO_CHILD_EVENT)
    {
        int rc;
        struct watcherContext *watcherPara = (struct watcherContext*)watcherCtx;
        // block the threads
        pthread_mutex_lock(watcherPara->lock);
        rc = zoo_wget_children(wzh, "/election", zoo_wget_children_watcher, watcherCtx, NULL);
        if (rc)
        {
            fprintf(stderr, "Error %d for zoo_wget_children\n", rc);
        }
        check_leader(watcherPara->cur_view, watcherPara->znode_path);
        pthread_mutex_unlock(watcherPara->lock); 
    }
}

int start_zookeeper(view* cur_view, int *zoo_fd, pthread_mutex_t *lock)
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

    char path_buffer[512];
    rc = zoo_create(zh, "/election/guid-n_", NULL, -1, &ZOO_OPEN_ACL_UNSAFE, ZOO_SEQUENCE|ZOO_EPHEMERAL, path_buffer, 512);
    if (rc)
    {
        fprintf(stderr, "Error %d for zoo_create\n", rc);
    }
    char znode_path[512];
    get_znode_path(path_buffer, znode_path);

    check_leader(cur_view, path_buffer);
    struct watcherContext *watcherPara = (struct watcherContext *)malloc(sizeof(struct watcherContext));
    strcpy(watcherPara->znode_path, znode_path);
    watcherPara->lock = lock;
    watcherPara->cur_view = cur_view;

    rc = zoo_wget_children(zh, "/election", zoo_wget_children_watcher, (void*)watcherPara, NULL);
    if (rc)
    {
        fprintf(stderr, "Error %d for zoo_wget_children\n", rc);
    }
    return 0;
}

int initialize_node(node* my_node,list* excluded_fd,const char* log_path, void (*user_cb)(size_t data_size,void* data,void* arg), void* db_ptr,void* arg){

    int flag = 1;

    dare_server_input_t input = {
        .log = stdout,
        .peer_pool = my_node->peer_pool,
        .group_size = my_node->group_size,
        .server_idx = my_node->node_id
    };

    if (0 != dare_server_init(&input)) {
        err_log("CONSENSUS MODULE : Cannot init dare\n");
        goto initialize_node_exit;
    }

    my_node->cur_view.view_id = 1;
    my_node->cur_view.req_id = 0;
    my_node->cur_view.leader_id = 9999;

    zoo_port = my_node->zoo_port;
    int *zoo_fd = (int*)malloc(sizeof(int));
    start_zookeeper(&my_node->cur_view, zoo_fd, &my_node->lock);
    
    listAddNodeTail(excluded_fd, (void*)zoo_fd);

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
                sprintf(sys_log_path,"%s/node-%u-consensus-sys.log",log_path,my_node->node_id);
                my_node->sys_log_file = fopen(sys_log_path,"w");
                free(sys_log_path);
            }
            if(NULL==my_node->sys_log_file && (my_node->sys_log || my_node->stat_log)){
                err_log("CONSENSUS MODULE : System Log File Cannot Be Created.\n");
            }
    }

    my_node->consensus_comp = NULL;

    my_node->consensus_comp = init_consensus_comp(my_node,&my_node->lock,my_node->config.measure_latency,
            my_node->node_id,my_node->sys_log_file,my_node->sys_log,
            my_node->stat_log,my_node->db_name,db_ptr,my_node->group_size,
            &my_node->cur_view,&my_node->highest_to_commit,&my_node->highest_committed,
            &my_node->highest_seen,user_cb,arg);
    if(NULL==my_node->consensus_comp){
        goto initialize_node_exit;
    }

    pthread_create(&my_node->rep_thread,NULL,handle_accept_req,my_node->consensus_comp);
    
    flag = 0;
initialize_node_exit:

    return flag;
}

void rsm_op(node* my_node, size_t ret, void *buf, output_peer_t* output_peers)
{
    consensus_submit_request(my_node->consensus_comp,ret,buf,output_peers);
    return;
}

uint32_t get_leader_id(node* my_node)
{
    return my_node->cur_view.leader_id;
}

uint32_t get_group_size(node* my_node)
{
    return my_node->group_size;
}

node* system_initialize(node_id_t node_id,list* excluded_fd,const char* config_path, const char* log_path, void(*user_cb)(size_t data_size,void* data,void* arg), void* db_ptr,void* arg){

    node* my_node = (node*)malloc(sizeof(node));
    memset(my_node,0,sizeof(node));
    if(NULL==my_node){
        goto exit_error;
    }

    my_node->node_id = node_id;
    myid = node_id;
    my_node->db_ptr = db_ptr;

    if(pthread_mutex_init(&my_node->lock,NULL)){
        err_log("CONSENSUS MODULE : Cannot Init The Lock.\n");
        goto exit_error;
    }

    if(consensus_read_config(my_node,config_path)){
        err_log("CONSENSUS MODULE : Configuration File Reading Failed.\n");
        goto exit_error;
    }


    if(initialize_node(my_node,excluded_fd,log_path,user_cb,db_ptr,arg)){
        err_log("CONSENSUS MODULE : Network Layer Initialization Failed.\n");
        goto exit_error;
    }

    return my_node;

exit_error:
    if(NULL!=my_node){
    }

    return NULL;
}