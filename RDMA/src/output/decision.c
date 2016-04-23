/*
Author: Jingyu
Date: 	April 5, 2016
Description: The function do_decision will consider the hashvalues from all nodes, then make a decision about whether to launch a restore process.
*/

#include "../include/output/output.h"
#include "../include/util/debug.h"


//This function will count the number of nodes whose hashvalue is the same as aim_hash
int count_hash(output_peer_t* output_peers, int group_size, uint64_t aim_hash){
	int i=0;
	int cnt=0;
	for ( i=0;i<group_size;i++){
		if (output_peers[i].hash == aim_hash){
			cnt++;
		}
	}	
	return cnt;
}

/*
This function will return the maximum number of a sub-set whose hash value are the same.
For example, if the group is (1,1,1,2,2), the function will return 3. And
(1,2,3,4,5) => 1
(1,1,2,2,3) => 2 
*/
int major_count_hash(output_peer_t* output_peers, int group_size){
	int i=0;
	int ret=0;
	int max_cnt=0;
	uint64_t aim_hash=0;
	for ( i=0;i<group_size;i++){
		aim_hash = output_peers[i].hash;
		ret = count_hash(output_peers, group_size, aim_hash);	
		if (ret>max_cnt){
			max_cnt = ret;
		}
	}
	return max_cnt;
}

uint64_t get_master_hash(output_peer_t* output_peers, int group_size){
	uint64_t master_hash=0;
	for (int i=0; i<group_size; i++){
		if (output_peers[i].leader_id == output_peers[i].node_id){
			master_hash = output_peers[i].hash;
			return master_hash;
		}
	}

	return master_hash;
}

// make dicision and trigger a restore cmd to guard.py
int do_decision(output_peer_t* output_peers, int group_size){
	int i=0;
	int threshold = 0; // threshold for majority
	int con_num = 0; // number of nodes whose hashvalue is same as master, including master itself.

	uint64_t master_hash = get_master_hash(output_peers,group_size);
	int major_cnt =0; // number of majority.
	int ret=0;
	// It provides a best effor decision.
	// If one of hash is 0, just return.
	int zero_count = 0;
	for (i = 0; i < group_size; i++){
		debug_log("[do_decision] leader_id:%u, node_id: %u, hashval: 0x%"PRIx64" hash_index:%ld\n",
			output_peers[i].leader_id,
			output_peers[i].node_id,
			output_peers[i].hash,
			output_peers[i].hash_index);
		if (0L == output_peers[i].hash){
			zero_count++;
		}
	}
	if (zero_count){
		debug_log("[do_decision] failed to make decision since one of hash is 0\n");
		// 0 means do nothing.
		return 0;
	}
	threshold = group_size/2 +1;
	/*
	Design Discuss:
	1. In this function, I need know who is the leader. Because Decision 3,4 need know whether the hash of leader is same as others.
	2. Right now, there is an assumption that leader_id is 0.
	3. [Solved] by calling get_master_hash()
	*/
	con_num = count_hash(output_peers,group_size,master_hash);
	if (con_num == group_size){ // D.0 all hash are the same.
		debug_log("[do_decision] D.0 All hash are the same (Nothing to do)\n");
		ret=0;
		return ret;
	}
	if (con_num >= threshold ){ // // D.1 H(header) == H(major).
		debug_log("[do_decision] D.1 Minority need redo.\n");
		ret=1;
		return ret;	
	}
	major_cnt = major_count_hash(output_peers, group_size); 
	if (major_cnt >= threshold){ // D2. H(header) != H(major).
    	debug_log("[do_decision] D.2 Master and Minority need redo.\n"); 
		ret=2;
		return ret;   
    }
	// consensus failed
	debug_log("[hash_consensus] D.3 All nodes need redo.\n");
	ret = 3;
	return ret;
}

