#ifndef DARE_SERVER_H
#define DARE_SERVER_H 

#include "dare_log.h"
#include "dare.h"
#include "../replica-sys/node.h"

#define INIT_LEADER 0
#define FIXED_LEADER 1

/**
 * The state identifier (SID)
 * the SID is a 64-bit value [TERM|L|IDX], where
    * TERM is the current term
    * L is the leader flag, set when there is a leader
    * IDX is the index of the server that cause the last SID update
 */
/* The IDX consists of the 8 least significant bits (lsbs) */
#define SID_GET_IDX(sid) (uint8_t)((sid) & (0xFF))
#define SID_SET_IDX(sid, idx) (sid) = (idx | ((sid >> 8) << 8))
/* The L flag is the 9th lsb */
#define SID_GET_L(sid) ((sid) & (1 << 8))
#define SID_SET_L(sid) (sid) |= 1 << 8
#define SID_UNSET_L(sid) (sid) &= ~(1 << 8)
/* The TERM consists of the most significant 55 bits */
#define SID_GET_TERM(sid) ((sid) >> 9)
#define SID_SET_TERM(sid, term) (sid) = (((term) << 9) | ((sid) & 0x1FF))


struct server_t {
    void *ep;               // endpoint data (network related)
};
typedef struct server_t server_t;

struct dare_server_input_t {
    FILE *log;
    uint32_t group_size;
    uint32_t *server_idx;
    view *cur_view;
    struct sockaddr_in *my_address;
    int hb_on;
    double hb_period;
};
typedef struct dare_server_input_t dare_server_input_t;

struct dare_server_data_t {
    dare_server_input_t *input;
    
    view* cur_view;
    struct sockaddr_in* my_address;
    
    server_config_t config; // configuration 
    
    dare_log_t  *log;       // local log (remotely accessible)
    struct ev_loop *loop;
};
typedef struct dare_server_data_t dare_server_data_t;

/* ================================================================== */

int dare_server_init( dare_server_input_t *input );
int server_update_sid( uint64_t new_sid, uint64_t old_sid );
int dare_rdma_shutdown();

int is_leader();

#endif /* DARE_SERVER_H */
