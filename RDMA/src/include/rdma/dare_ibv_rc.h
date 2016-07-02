#ifndef DARE_IBV_RC_H
#define DARE_IBV_RC_H

#include <infiniband/verbs.h> /* OFED stuff */
#include "dare_ibv.h"

#define Q_DEPTH 8192
#define S_DEPTH 4096
#define S_DEPTH_ 4095

int rc_init();
void rc_free();

int post_send(uint32_t server_id, void *buf, uint32_t len, struct ibv_mr *mr, enum ibv_wr_opcode opcode, rem_mem_t *rm, int send_flags, int poll_completion);

/* QP interface */
int rc_connect_server(uint8_t idx, uint16_t dlid, uint8_t *dgid);

#endif /* DARE_IBV_RC_H */
