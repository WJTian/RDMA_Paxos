#ifndef SHM_H
#define SHM_H
#include "../util/common-structure.h"

struct shm_data_t {
	void* shm[MAX_SERVER_COUNT];
	log_t* log;
};

typedef struct shm_data_t shm_data;

extern shm_data shared_memory;

#ifdef __cplusplus
extern "C" {
#endif

	void init_shm(node_id_t node_id, int size);

#ifdef __cplusplus
}
#endif

#endif
