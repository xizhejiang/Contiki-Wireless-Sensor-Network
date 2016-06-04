//
// Created by JIANGXizhe on 4/14/16.
//

#ifndef CONTIKI_MSG_H

#define CONTIKI_MSG_H

#include <stdint.h>
#include <stdbool.h>
//#include "addtional.h"

#define MAX_MSG_ENTRY 4

/**
 * the type of the message
 */
enum {
    NORMAL = 5,
    SUBSCRIBE = 6,
    UNSUBSCRIBE = 7,
    NONE = 8

};
/**
 * the aggregate condtion of the data in the sensor node
 */
enum{
    AVERAGE = 9,
    MAX = 10,
    EACH = 11,
    NONE_CONDITION = 12
};
/**
 * the message of aggregated date to send to the base station
 */
typedef struct Runicast_msg{
    uint16_t seqno;
    uint8_t native_sen_type;
    uint32_t timestamp;
    uint8_t value;
    rimeaddr_t last_hop;
    rimeaddr_t source;
    uint8_t hops;
}Runicast_msg;
/**
 *
 * broadcast msg only sends to the neighbor for one hop
 */
typedef struct broadcast_msg{
    uint8_t seqno;
    uint8_t native_sen_type;
    rimeaddr_t last_hop;
    rimeaddr_t source;
    rimeaddr_t base_station;
    uint8_t hops;
    //uint16_t total_cost;
    uint8_t frequency;
    uint8_t subscribe_type;
    uint8_t msg_type;
    uint8_t condition;
    uint8_t assemble_count;
    //uint32_t lamport_clock;
    uint32_t create_timestamp;
}Broadcast_msg;
/*
 *  list of msg to broadcast, if it is base station add it to the broadcast list
 */
typedef struct Br_msg_list{
    struct Br_msg_list *next;
    uint32_t local_create_tp;
    Broadcast_msg broadcast_msg;
}Br_msg_list;

/**
 *  list of msg to forward/runicast
 */
typedef struct Ru_msg_list{
    struct Ru_msg_list *next;
    uint32_t local_create_tp;
    Runicast_msg runicast_msg;

}Ru_msg_list;


#endif //CONTIKI_MSG_H
