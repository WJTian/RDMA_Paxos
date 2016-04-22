#include "../include/output/output.h"
#include "../include/output/crc64.h"
#include "../include/util/debug.h"

//[TODO] The following extern will be removed.
// output_handler will be get from different fd.
//output_handler_t output_handler;

//[TODO] The init will consider how to assign a output_handler for a certain fd.
void init_output(int fd)
{
	// output_handler.output_list = listCreate();
	// output_handler.count = 0;
	// output_handler.prev_offset = 0;
	// pthread_mutex_init(&output_handler.lock, NULL);
	// output_handler.hash = 0;

	// output_handler.hash_buffer_curr =0;
	// memset(output_handler.hash_buffer,0,sizeof(output_handler.hash_buffer));
	// return;
}

static int min(int a, int b){
	return a<b?a:b;
}


output_handler_t* get_output_handler_by_fd(int fd){
	debug_log("[get_output_handler_by_fd] has not been implemented yet. fd:%d\n",fd);
	return NULL;
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