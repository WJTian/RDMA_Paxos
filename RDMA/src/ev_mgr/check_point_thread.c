#include "../include/ev_mgr/check_point_thread.h"
#include "../include/util/debug.h"

// Libevent
#include "event.h"
void * check_point_thread_start(void* argv){
	// libevent init
	struct event_base* base = event_base_new();
	// libevent read_handler() will callit
	int ret=disconnct_inner();
	debug_log("[check_point] started.");
	event_base_free(base);
	return NULL;
}
