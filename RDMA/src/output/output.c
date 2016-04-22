#include "../include/output/output.h"
#include "../include/output/crc64.h"
#include "../include/output/adlist.h"
#include "../include/util/debug.h"


void init_output_mgr(){
	output_manager_t *output_mgr = get_output_mgr();
	debug_log("[init_output_mgr] output_mgr is inited at %p\n",output_mgr);
}

void deinit_output_mgr(){
	output_manager_t *output_mgr = get_output_mgr();
	if (output_mgr){ 
		debug_log("[deinit_output_mgr] output_mgr(%p) will be freed\n",output_mgr);
		//[TODO] free output_handler first
		int num = deinit_fd_handler(output_mgr);
		debug_log("[deinit_output_mgr] %d fd_handler have been freed\n",num);
		free(output_mgr);
		output_mgr=NULL;
	}
}

// inner init function will not be shown to outside.

void init_fd_handler(output_manager_t *output_mgr){
	if (NULL==output_mgr){
		return;
	}
	memset(output_mgr->fd_handler,NULL,sizeof(output_mgr->fd_handler));
}

// return how many fd handler has been freed.
int deinit_fd_handler(output_manager_t *output_mgr){
	if (NULL==output_mgr){
		return 0;
	}
	int cnt=0;
	for (int i=0;i<MAX_FD_SIZE;i++){
		output_handler_t* ptr = output_mgr->fd_handler[i];
		if (ptr){
			delete_output_handler(ptr);
			ptr=NULL;
			output_mgr->fd_handler[i] = NULL;
			cnt++;
		}
	}
	return cnt;
}

output_manager_t * get_output_mgr(){
	static output_manager_t * inner_output_mgr=NULL; // This is a singleton of output_mgr
	if (NULL == inner_output_mgr){
		//init output manager
		inner_output_mgr = (output_manager_t *)malloc(sizeof(output_manager_t *)); // it will be freed in deinit_output_mgr
		init_fd_handler(inner_output_mgr);
	}
	return inner_output_mgr;
}

static int min(int a, int b){
	return a<b?a:b;
}

// malloc a output_handler_t for this fd
output_handler_t* new_output_handler(int fd){
	output_handler_t* ptr = (output_handler_t*)malloc(sizeof(output_handler_t));
	ptr->fd = fd;
	ptr->count = 0;
	ptr->output_list = listCreate(); 
	ptr->hash = 0L;
	memset(ptr->hash_buffer,0,sizeof(ptr->hash_buffer));
	ptr->hash_buffer_curr = 0;
	return ptr;
}

void delete_output_handler(output_handler_t* ptr){
	if (NULL == ptr){
		return;
	}
	listRelease(ptr->output_list);
	ptr->output_list = NULL;
	free(ptr);
}

output_handler_t* get_output_handler_by_fd(int fd){
	debug_log("[get_output_handler_by_fd] fd: %d \n",fd);
	output_manager_t *output_mgr = get_output_mgr();
	if (NULL == output_mgr){
		return NULL;
	}

	output_handler_t* ptr = output_mgr->fd_handler[i];
	if (NULL == ptr){ // At the first time, output_handler_t need to be inited.
		ptr = new_output_handler(fd);
		output_mgr->fd_handler[i] = ptr ; 
	}
	return output_mgr->fd_handler[i];
}

//accept a buff with size, I will store into different connection (fd).
//And return n number of hash values.
int store_output(int fd, const unsigned char *buff, ssize_t buff_size)
{
	debug_log("[store_output] fd: %d, input buff_size: %zu \n",fd, buff_size);
	int retval=-1;
	if (NULL==buff || 0 == buff_size){ // nothing to be done, but it is not a error
		retval=0;
		return retval;
	}
	// A output_handler will be get from different fd
	output_handler_t* output_handler = get_output_handler_by_fd(fd);
	if (NULL == output_handler){// error
		debug_log("[store_output] fd:%d, get_output_handler_by_fd error. \n",fd);
		return retval;
	}
	// The following code will put buff into output_handler.hash_buffer
	// Please do not delete the comment unless you list your reason at here. ^^.Thanks.
	int push_size =0;
	retval=0; // default value means no hash value is generated.
	while (push_size < buff_size){ // Which means the input buff has not been handled.
		int left_space = HASH_BUFFER_SIZE - output_handler->hash_buffer_curr;
		int wait_size = buff_size - push_size;
		int actual_size = min(left_space,wait_size); 
		unsigned char * dest_ptr = output_handler->hash_buffer + output_handler->hash_buffer_curr;
		memcpy(dest_ptr,buff,actual_size);
		output_handler->hash_buffer_curr+=actual_size;
		left_space = HASH_BUFFER_SIZE - output_handler->hash_buffer_curr;
		push_size+=actual_size;
		if (0==left_space){ // The hash buffer is full.
			output_handler->hash = crc64(output_handler->hash,output_handler->hash_buffer,HASH_BUFFER_SIZE);
			// curr is clear, since new hash is generated.
			output_handler->hash_buffer_curr=0;
			debug_log("[store_output] fd:%d, hash is generated, hash:0x%"PRIx64"\n",fd, output_handler->hash);

			// add hash into output_list;
			uint64_t *new_hash = (uint64_t*)malloc(sizeof(uint64_t));
			*new_hash = output_handler->hash;
			listAddNodeTail(output_handler->output_list, (void*)new_hash);
			debug_log("[store_output] fd:%d, hash is putted into output_list. count:%ld, hash:0x%"PRIx64"\n", 
				fd, output_handler->count, output_handler->hash);
			output_handler->count++;
			retval++; // one hash value is generated.
		}
	}
	return retval;
}

void get_output(int fd, long hash_index)
{
	//socket_pair* ret = NULL;
	//HASH_FIND_INT(ev_mgr->hash_map, &fd, ret);


	//HASH_FIND_INT(output_handler->hash_table, &ret->s_p, ) find the corresponding hash table
	//listNode * node = listIndex(list *list, index);
	//return listNodeValue(node);
}


void del_output(int fd)
{
	//socket_pair* ret = NULL;
	//HASH_FIND_INT(ev_mgr->hash_map, &fd, ret);


	//HASH_FIND_INT(output_handler->hash_table, &ret->s_p, ) find the corresponding hash table
	//listNode * node = listIndex(list *list, index);
	//return listNodeValue(node);
}