#include "../include/util/clock.h"

#define BILLION 1000000000L

list *clock_init()
{
	list *clock_list = listCreate();
	return clock_list;
}

void clock_add(list *clock_list)
{
	struct timespec *node_time = (struct timespec *)malloc(sizeof(struct timespec));
	clock_gettime(CLOCK_MONOTONIC, node_time);
	listAddNodeTail(clock_list, (void*)node_time);
}

void clock_display(FILE* output, list *clock_list)
{
	uint64_t diff;
	listIter *iter = listGetIterator(clock_list,AL_START_HEAD);
	struct timespec start_time, end_time;
	struct timespec *node_time;
	listNode *node;
	char tmp[64], str[256];
	memset(str, 0, sizeof(str));
	while ((node = listNext(iter)) != NULL)
	{
		node_time = (struct timespec *)listNodeValue(node);
		end_time.tv_sec = node_time->tv_sec;
		end_time.tv_nsec = node_time->tv_nsec;
		if (node != listIndex(clock_list, 0))
		{
			diff = BILLION * (end_time.tv_sec - start_time.tv_sec) + end_time.tv_nsec - start_time.tv_nsec;
			sprintf(tmp, "%llu;", (long long unsigned int)diff);
			strcat(str, tmp);
		}
		start_time.tv_sec = end_time.tv_sec;
		start_time.tv_nsec = end_time.tv_nsec;
	}
	safe_rec_log(output, "%s\n", str);
	listRelease(clock_list);
}