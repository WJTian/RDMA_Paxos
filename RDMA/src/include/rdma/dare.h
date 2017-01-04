#include "../util/debug.h"

#ifndef DARE_H_
#define DARE_H_

#define MAX_SERVER_COUNT 7
#define NOW 0.000000001
#define PAGE_SIZE 4096

/**
 *  UD message types 
 */
/* Initialization messages */
#define RC_SYN      1
#define RC_SYNACK   2
#define RC_ACK      3
/* Config messages */
/* Config messages */
#define JOIN        211
#define DESTROY     213
#define CFG_REPLY   214

#endif /* DARE_H_ */