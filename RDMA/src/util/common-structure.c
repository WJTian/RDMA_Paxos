#include "../include/util/common-structure.h"

int view_stamp_comp(view_stamp* op1,view_stamp* op2){
    if(op1->view_id<op2->view_id){
        return -1;
    }else if(op1->view_id>op2->view_id){
        return 1;
    }else{
        if(op1->req_id>op2->req_id){
            return 1;
        }else if(op1->req_id<op2->req_id){
            return -1;
        }else{
            return 0;
        }
    }
}

/* view stamp to long */
uint64_t vstol(view_stamp* vs){
    uint64_t result = ((uint64_t)vs->req_id)&0xFFFFFFFFl;
    uint64_t temp = (uint64_t)vs->view_id&0xFFFFFFFFl;
    result += temp<<32;
    return result;
};

view_stamp ltovs(uint64_t record_no){
    view_stamp vs;
    uint64_t temp;
    temp = record_no&0x00000000FFFFFFFFl;
    vs.req_id = temp;
    vs.view_id = (record_no>>32)&0x00000000FFFFFFFFl;
    return vs;
}

ssize_t original_write(int fd, const void *buf, size_t count)
{
    typedef ssize_t (*orig_write_type)(int, const void *, size_t);
    static orig_write_type orig_write;
    if (!orig_write)
        orig_write = (orig_write_type) dlsym(RTLD_NEXT, "write");
    ssize_t ret = orig_write(fd, buf, count);

    return ret;
}

int original_close(int fildes)
{
    typedef int (*orig_close_type)(int);
    static orig_close_type orig_close;
    if (!orig_close)
        orig_close = (orig_close_type) dlsym(RTLD_NEXT, "close");
    int ret = orig_close(fildes);
    return ret;
}

int original_accept(int socket, struct sockaddr *address, socklen_t *address_len)
{
    typedef int (*orig_accept_type)(int, struct sockaddr *, socklen_t *);
    static orig_accept_type orig_accept;
    if (!orig_accept)
        orig_accept = (orig_accept_type) dlsym(RTLD_NEXT, "accept");

    int ret = orig_accept(socket, address, address_len);

    return ret;
}