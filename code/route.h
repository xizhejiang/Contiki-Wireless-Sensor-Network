//
// Created by JIANGXizhe on 4/18/16.
//

#ifndef CONTIKI_ROUTE_H
#define CONTIKI_ROUTE_H
#include "contiki.h"
#include "net/rime.h"
#include <stdint.h>
#include <stdbool.h>
//#include "addtional.h"
#include "route.h"

#define MAX_ROUTING_ENTRY 8

/**
 * the routing table of the node
 */
typedef struct Rt_Entry{
    rimeaddr_t base;
    rimeaddr_t next_hop;
    int total_hops;
    int total_cost;
    uint32_t timestamp;
}Rt_Entry;

typedef struct Rt_table{
    struct Rt_table *next;
    Rt_Entry rt_entry;
}Rt_table;

#endif //CONTIKI_ROUTE_H
