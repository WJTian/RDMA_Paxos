static int post_send(uint8_t server_id, int qp_id, void *buf, uint32_t len, struct ibv_mr *mr, enum ibv_wr_opcode opcode, int signaled, rem_mem_t rm, int *posted_sends );