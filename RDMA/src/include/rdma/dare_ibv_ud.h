#ifndef DARE_IBV_UD_H
#define DARE_IBV_UD_H

#include "dare_ibv.h"
#include "dare_config.h"

/* ================================================================== */
/* UD messages */

struct ud_hdr_t {
    uint32_t type;
    size_t length;
};
typedef struct ud_hdr_t ud_hdr_t;

struct reconf_rep_t {
    ud_hdr_t   hdr;
    uint32_t   idx;
};
typedef struct reconf_rep_t reconf_rep_t;

struct rc_syn_t {
    ud_hdr_t hdr;
    rem_mem_t log_rm;
    uint32_t idx;
    uint16_t lid;
    uint8_t gid[16];
    uint8_t data[0];    // log ctrl QPN
};
typedef struct rc_syn_t rc_syn_t;

struct rc_destroy_t {
    ud_hdr_t hdr;
    uint32_t idx;  
};
typedef struct rc_destroy_t rc_destroy_t;

/* ================================================================== */ 

void ud_get_message();
int ud_join_cluster();

#endif /* DARE_IBV_UD_H */
