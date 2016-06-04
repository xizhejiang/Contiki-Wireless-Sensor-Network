//
// Created by JIANGXizhe on 4/14/16.
//

#ifndef CONTIKI_NODE_H
#define CONTIKI_NODE_H

#include "contiki.h"
#include "net/rime.h"
#include <stdint.h>
#include <stdbool.h>
//#include "addtional.h"
#include "route.h"
/**
 * node types
 */
enum {
    BASE_STATION = 0,
    LIGHT = 1,
    SOUND = 2,
    HEAT = 3,
    NONE_TYPE = 4

};
/**
 * the base staion or the the sensor node
 */
typedef struct Node{
    rimeaddr_t sid;
    uint8_t sen_type;
    //Rt_table *head;
    uint32_t frequency;
    bool subscribe;
    uint8_t condition;
    uint8_t assemble_count;
    //uint32_t lamport_difference;
    uint8_t sub_type;

}Node;

#endif //CONTIKI_NODE_H
