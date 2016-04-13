#ifndef CLOCK_H
#define CLOCK_H
#include "../output/adlist.h"
#include "./common-header.h"

list *clock_init();
void clock_add(list *clock_list);
void clock_display(FILE *output, list *clock_list);


#endif 