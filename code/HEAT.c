//
// Created by JIANGXizhe on 4/18/16.
//
#include "contiki.h"
#include "lib/list.h"
#include "lib/memb.h"
#include "lib/random.h"
#include "net/rime.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include "msg.h"
#include "node.h"
#include "route.h"
#include "dev/button-sensor.h"

#define MAX_BC_INTERVAL 4
#define RU_INTERVAL 3
#define MAX_RETRANSMISSIONS 4   /**<the max times of retransmissions*/
#define FREQUENCY 5
#define ASSEMBLE_COUNT 3
#define CONDITION_DEFINE MAX
/**
 * need to be commented below
 */
#include "addtional.h"
/*
 * need to be commented above
 */
#define SUBSCRIBE_INIT LIGHT
#define NODE_INIT HEAT
#define STALE_PERIORD 20
#define CHECK_STALE_PERIORD 4
#define BR_VALID_PERIORD 30
static int ifinit = 0;          /**<global variable to control and make sure one-time operations*/
Node local_node;
static struct broadcast_conn broadcast;   /**<connection of broadcast*/
static struct runicast_conn runicast;     /**<connection of runicast*/
static uint8_t br_seqno = 0;
static uint8_t clct_data_seqno = 0;
static bool ru_send_success;
static Rt_table *current_route;
static uint8_t assemble_total = 0;
static int assemble_count = 0;
static uint8_t max = 0;


LIST(runicast_list);
LIST(broadcast_list);
LIST(routing_table);

MEMB(routing_table_memb, struct Rt_table, MAX_ROUTING_ENTRY);
MEMB(broadcast_list_memb, struct Br_msg_list, MAX_MSG_ENTRY);
MEMB(runicast_list_memb, Ru_msg_list, MAX_MSG_ENTRY);





PROCESS(broadcast_process, "Broadcast process"); /**<declare process for broadcasting*/
PROCESS(runicast_process, "runicast process"); /**<declare process for runicasting*/
PROCESS(remove_stale_process, "remove stale process"); /*remove statle routing entry*/
PROCESS(pr_buf_process, "print buf data process"); /*print all of the messages in the buffer*/
PROCESS(remove_br_process, "remove br process"); /*remove the invalid broadcasting entry*/

AUTOSTART_PROCESSES(&broadcast_process,&runicast_process,&remove_stale_process,&pr_buf_process,&remove_br_process);
//AUTOSTART_PROCESSES(&broadcast_process,&runicast_process);



/**
 * lamport clock implementation
 */
//uint32_t lamport_clock(){
//    return clock_seconds()+local_node.lamport_difference;
//}
//
//void update_lamport_clock(uint32_t timestamp){
//    if(timestamp>clock_seconds()){
//        local_node.lamport_difference = timestamp - clock_seconds();
//    }
//}

/**
 * create a broadcast msg from the argument given
 */
Br_msg_list* create_broadcast_msg(uint8_t native_sen_type, uint8_t msg_type, uint8_t frequency, uint8_t sub_type,
                                  rimeaddr_t last_hop,rimeaddr_t source,rimeaddr_t base_station,uint8_t hops,uint8_t condition,uint8_t assemble_count,uint32_t timestamp){
    struct Br_msg_list *broadcast_entry;
    broadcast_entry = memb_alloc(&broadcast_list_memb);
    if(broadcast_entry == NULL){
        printf("@broacast create can not alloc memoery\n");
        list_chop(broadcast_list);
    }
    //memset(broadcast_entry,0, sizeof(struct Br_msg_list));
    broadcast_entry->broadcast_msg.seqno = br_seqno;
    broadcast_entry->broadcast_msg.native_sen_type = native_sen_type;
    rimeaddr_copy(&broadcast_entry->broadcast_msg.last_hop, &last_hop);
    rimeaddr_copy(&broadcast_entry->broadcast_msg.source, &source);
    rimeaddr_copy(&broadcast_entry->broadcast_msg.base_station,&base_station);
    broadcast_entry->broadcast_msg.hops = hops;
    broadcast_entry->broadcast_msg.msg_type = msg_type;
    //  broadcast_entry->broadcast_msg.timestamp = CLOCK_SECOND;
    broadcast_entry->broadcast_msg.frequency = frequency;
    broadcast_entry->broadcast_msg.subscribe_type = sub_type;
    broadcast_entry->broadcast_msg.condition = condition;
    broadcast_entry->broadcast_msg.assemble_count = assemble_count;
    broadcast_entry->broadcast_msg.create_timestamp = timestamp;
    if(br_seqno>254){
        br_seqno = 0;
    }else{
        br_seqno++;
    }

    return broadcast_entry;
}
/**
 * find the best route to the base station
 */
Rt_table* find_best_route(){
    uint8_t min_hops = 255;
    Rt_table *rt_table;
    Rt_table *min_entry;
    for(rt_table = list_head(routing_table);rt_table!=NULL;rt_table = rt_table->next){
        if(rt_table->rt_entry.total_hops<min_hops){
            min_hops = rt_table->rt_entry.total_hops;
            min_entry = rt_table;
        }
    }
    if(min_entry == NULL){
        printf("[ERRO] ->cant find the best route\n");
    }
    return min_entry;
}
/**
 * update the broadcast buffer
 */
void update_br_entry(Br_msg_list *broadcast_entry, uint8_t native_sen_type, uint8_t msg_type, uint8_t frequency, uint8_t sub_type
        ,rimeaddr_t last_hop,rimeaddr_t source,rimeaddr_t base_station,uint8_t hops,uint8_t condition,uint8_t assemble_count,uint32_t timestamp){
    if(broadcast_entry == NULL){
        printf("NULL ENTRY INPUT\n");
    }else{
        //memset(broadcast_entry,0, sizeof(struct Br_msg_list));
        broadcast_entry->broadcast_msg.seqno = br_seqno;
        broadcast_entry->broadcast_msg.native_sen_type = native_sen_type;
        printf("last hop addr -> %u.%u\n",last_hop.u8[0],last_hop.u8[1]);
        rimeaddr_copy(&broadcast_entry->broadcast_msg.last_hop, &last_hop);
        rimeaddr_copy(&broadcast_entry->broadcast_msg.source, &source);
        rimeaddr_copy(&broadcast_entry->broadcast_msg.base_station,&base_station);
        broadcast_entry->broadcast_msg.hops = hops;
        broadcast_entry->broadcast_msg.msg_type = msg_type;
        //  broadcast_entry->broadcast_msg.timestamp = CLOCK_SECOND;
        broadcast_entry->broadcast_msg.frequency = frequency;
        broadcast_entry->broadcast_msg.subscribe_type = sub_type;
        broadcast_entry->broadcast_msg.condition = condition;
        broadcast_entry->broadcast_msg.assemble_count = assemble_count;
        broadcast_entry->broadcast_msg.create_timestamp = timestamp;
    }
}

void remove_all_br_entry(){
    while(list_length(broadcast_list)>0){
        memb_free(&broadcast_list_memb,list_chop(broadcast_list));
    }
}
/**
 * add broadcast entry in the broadcast buffer, if the message type exits , the message will be updated, if
 * not exist, a new entry to the broadcast buffer will be created
 */
void add_broadcast_entry(uint8_t native_sen_type, uint8_t msg_type, uint8_t frequency, uint8_t sub_type
        ,rimeaddr_t last_hop,rimeaddr_t source,rimeaddr_t base_station,uint8_t hops,uint8_t condition,uint8_t assemble_count, uint32_t timestamp) {
    //printf("in add broadcast entry -> msg_type ->%u\n",msg_type);
    bool find = false;
    struct Br_msg_list *br_msg_list;
    if (list_length(broadcast_list) > 0) {
        for (br_msg_list = list_head(broadcast_list); br_msg_list != NULL; br_msg_list = br_msg_list->next) {
            if (br_msg_list->broadcast_msg.subscribe_type == sub_type&&br_msg_list->broadcast_msg.create_timestamp<timestamp) {
                update_br_entry(br_msg_list, local_node.sen_type, msg_type, frequency, sub_type, last_hop, source,
                                base_station, hops, condition, assemble_count,timestamp);
                printf("[UPDATE_BR_ENTRY]: sub_type->%u native_type->%u\n", br_msg_list->broadcast_msg.subscribe_type,
                       br_msg_list->broadcast_msg.native_sen_type);
                find = true;
                break;
            }
        }

    }
    if (!find) {
        if (list_length(broadcast_list) > MAX_MSG_ENTRY - 1) {
            memb_free(&broadcast_list_memb, list_chop(broadcast_list));
            printf("[BUF FULL]chop msg\n");
        }
        Br_msg_list *temp;
        temp = create_broadcast_msg(local_node.sen_type, msg_type, frequency, sub_type, last_hop, source, base_station,
                                    hops, condition, assemble_count,clock_seconds());
        if (temp != NULL) {
            list_add(broadcast_list, temp);
            printf("[ADD-TO-BRLIST] sub type %u list length %u\n", temp->broadcast_msg.subscribe_type,
                   list_length(broadcast_list));
            print_br_list();

        }


    }
}
/**
 * print broadcast buffer list
 */
void print_br_list(){
    Br_msg_list *br_msg_list;
    for(br_msg_list = list_head(broadcast_list);br_msg_list!=NULL;br_msg_list = br_msg_list->next){
        printf("print br list -> base station->%u.%u msg_type %u\n",br_msg_list->broadcast_msg.base_station.u8[0],br_msg_list->broadcast_msg.base_station.u8[1],
               br_msg_list->broadcast_msg.msg_type);
    }
}
/**
 * subscribe a certain type of sensor
 */
void subscribe(uint8_t sub_sen_type, uint8_t frequency, uint8_t condition, uint8_t assemble_count){

    add_broadcast_entry(BASE_STATION,SUBSCRIBE,frequency,sub_sen_type,local_node.sid,local_node.sid,local_node.sid,0,CONDITION_DEFINE,assemble_count,clock_seconds());
}
/**
 * unsubscribe a certain type of sensor
 */
void unsubscribe(uint8_t sen_type){
    add_broadcast_entry(BASE_STATION,UNSUBSCRIBE,MAX_BC_INTERVAL,sen_type,local_node.sid,local_node.sid,local_node.sid,0,CONDITION_DEFINE,0,clock_seconds());
}

/**
 * increase the hop by 1
 */

void add_hops(Broadcast_msg *broadcast_msg){
    broadcast_msg->hops++;
}

bool compare_id(rimeaddr_t a1, rimeaddr_t a2) {
    if(rimeaddr_cmp(&a1,&a2)){
        return true;
    }
    return false;
}
/**
 * send the broadcast list to the neighbor
 */
void send_br_list(struct broadcast_conn *broadcastConn){
    Br_msg_list *br_msg_list;
    Br_msg_list *remove;
    if(list_length(broadcast_list)>0){
        for(br_msg_list = list_head(broadcast_list);br_msg_list!=NULL;){
            packetbuf_copyfrom(&br_msg_list->broadcast_msg,sizeof(Broadcast_msg));
            printf("[BC] @%d @%u.%u base station@%u.%u\n", clock_seconds(),rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
                   br_msg_list->broadcast_msg.base_station.u8[0],br_msg_list->broadcast_msg.base_station.u8[1]);
            broadcast_send(broadcastConn);
            packetbuf_clear();
            print_br_list();
            remove = br_msg_list;
            br_msg_list = br_msg_list->next;
            if(local_node.sen_type == BASE_STATION && (remove->broadcast_msg.msg_type==SUBSCRIBE||
                                                       remove->broadcast_msg.msg_type==UNSUBSCRIBE)){
            }else if(remove!=NULL&&list_length(broadcast_list)>1){
                list_remove(broadcast_list,remove);
                memb_free(&broadcast_list_memb, remove);
                printf("[DELET-BR-MSG] msg sub_type %u msg_type %u br list length %u\n",remove->broadcast_msg.subscribe_type,
                       remove->broadcast_msg.msg_type,list_length(broadcast_list));
                remove = NULL;
                break;
            }
        }
    }

}
/**
 * remove all of the runicast message buffered
 */
void remove_all_runicast_entry(){
    while(list_length(runicast_list)>0){
        list_chop(runicast_list);
    }
};
/**
 * the sensor node subscribed will send the data to the base station via the next hop according to the routing table
 */
void publish(struct runicast_conn *runicast){
    Ru_msg_list *ru_msg_list;
    Rt_table *chosen_route;
    if(list_length(runicast_list)>0){
        for(ru_msg_list = list_head(runicast_list);ru_msg_list!=NULL;){
            chosen_route = find_best_route();
            current_route = chosen_route;
            if(current_route!=NULL){
                packetbuf_copyfrom(&ru_msg_list->runicast_msg, sizeof(Runicast_msg));
                printf("[RU] @%d-@%u.%u->@%u.%u\n", clock_seconds(),rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
                       chosen_route->rt_entry.next_hop.u8[0],chosen_route->rt_entry.next_hop.u8[1]);
                runicast_send(runicast,&chosen_route->rt_entry.next_hop,MAX_RETRANSMISSIONS);
                packetbuf_clear();
                Ru_msg_list *to_remove;
                to_remove = ru_msg_list;
                ru_msg_list = ru_msg_list->next;
//                if(ru_send_success == true){
//                    list_remove(runicast_list,to_remove);
//                    memb_free(&runicast_list_memb,to_remove);
//                    printf("[DELET-RU-MSG]\n");
//                }
            }else{
                printf("[ERRO] current route is none");
            }


        }
    }
    remove_all_runicast_entry();
}
/**
 * publish a single message
 */
void single_publish(struct runicast_conn *runicast,Runicast_msg runicast_msg){

    packetbuf_copyfrom(&runicast_msg, sizeof(Runicast_msg));
    printf("[RU] timestamp @%d seqno %d last hop address %u.%u\n", runicast_msg.timestamp,runicast_msg.seqno,runicast_msg.last_hop.u8[0],runicast_msg.last_hop.u8[1]);

    runicast_send(runicast,&find_best_route()->rt_entry.next_hop,MAX_RETRANSMISSIONS);
    packetbuf_clear();
}


/**
 * update the subscribtions
 */
void update_subscribtion( Node *node,Broadcast_msg *broadcast_msg){

    if(broadcast_msg->msg_type == SUBSCRIBE && node->sen_type == broadcast_msg->subscribe_type){

        printf("[UP_SUB]node %u been subscribed\n",node->sid.u8[0],node->sid.u8[1]);
        node->frequency = broadcast_msg->frequency;
        node->subscribe = true;
        node->condition = broadcast_msg->condition;
        node->assemble_count = broadcast_msg->assemble_count;

    }else if(broadcast_msg->msg_type == UNSUBSCRIBE && node->sen_type == broadcast_msg->subscribe_type){
        remove_all_br_entry();
        printf("[UP_UNSUB]node %u been unsubscribed\n",node->sid.u8[0],node->sid.u8[1]);
        node->frequency = MAX_BC_INTERVAL;
        node->subscribe = false;
        node->condition = NONE_CONDITION;
        node->assemble_count = 0;

    }

}
/**
 * add a routing entry to the routing table
 */
void add_routing_entry(Broadcast_msg *broadcast_msg){
    if(list_length(routing_table)>MAX_ROUTING_ENTRY-1){
        memb_free(&routing_table_memb,list_chop(routing_table));
    }
    Rt_table *rt_table;
    rt_table = memb_alloc(&routing_table_memb);
    if(rt_table == NULL){
        list_chop(routing_table);
    }
    //memset(rt_table,0, sizeof(Rt_table));
    rt_table->rt_entry.total_hops = broadcast_msg->hops;
    rimeaddr_copy(&rt_table->rt_entry.base,&broadcast_msg->base_station);
    rimeaddr_copy(&rt_table->rt_entry.next_hop,&broadcast_msg->source);
    rt_table->rt_entry.timestamp = clock_seconds();
    list_add(routing_table,rt_table);
    printf("[ADD-RT-ENTRY]-> base station %u.%u from source %u.%u\n",broadcast_msg->base_station.u8[0],broadcast_msg->base_station.u8[1]
            ,broadcast_msg->source.u8[0],broadcast_msg->source.u8[1]);

}

/**
 * update the routing table
 */
void check_update_rt_table(Broadcast_msg *broadcast_msg){
    bool contains = false;
    Rt_table *rt_table;
    if(list_length(routing_table) == 0){
        add_routing_entry(broadcast_msg);
    }else{
        for(rt_table = list_head(routing_table);rt_table!=NULL;rt_table = rt_table->next){
            if(rimeaddr_cmp(&rt_table->rt_entry.base,&broadcast_msg->base_station)){
                contains = true;
                /*in case of circle*/
                if(rt_table->rt_entry.total_hops>broadcast_msg->hops&&!rimeaddr_cmp(&rimeaddr_node_addr,&broadcast_msg->last_hop)
                   ||rimeaddr_cmp(&rt_table->rt_entry.next_hop,&broadcast_msg->source)){
                    rimeaddr_copy(&rt_table->rt_entry.next_hop,&broadcast_msg->source);
                    rt_table->rt_entry.total_hops = broadcast_msg->hops;
                    rt_table->rt_entry.timestamp = clock_seconds();
                    printf("[UPDATE-RT-TABLE]->find shorter path\n");
                }
            }
        }
        if(!contains){
            add_routing_entry(broadcast_msg);
        }
    }
}
/**
 * remove a certain routing entry
 */
void remove_route_entry(Rt_table *rt_table){
    list_remove(routing_table,rt_table);
    memb_free(&routing_table_memb,rt_table);
    printf("[ROMOVE-RT-ENTRY] @%u.%u",rt_table->rt_entry.next_hop.u8[0],rt_table->rt_entry.next_hop.u8[1]);
}
/**
 * create a broadcast message from the routing table
 */
void create_br_msg_from_RT_table(){
    Rt_table *rt_table = find_best_route();
    if(rt_table!=NULL){
        printf("[CRT-FROM-BR-LIST]->\n");
        add_broadcast_entry(local_node.sen_type,NORMAL,local_node.frequency,NONE_TYPE,rt_table->rt_entry.next_hop,rimeaddr_node_addr
                ,rt_table->rt_entry.base,rt_table->rt_entry.total_hops,NONE_CONDITION,0,clock_seconds());
    }else{
        printf("[ERROR] cant find best route\n");
    }

}
/**
 * add the runicast message to the runicast buffer
 */
void add_to_RU_list (Runicast_msg runicast_msg){
    if(list_length(runicast_list)>MAX_MSG_ENTRY-1){
        memb_free(&runicast_list_memb, list_chop(runicast_list));
    }
    Ru_msg_list *runicast_entry;
    runicast_entry = memb_alloc(&runicast_list_memb);
    printf("<alloc mem> runicast_entry alloc mem\n");
    if(runicast_entry == NULL){
        printf("runicast can alloc mem\n");
        list_chop(runicast_list);
    }
    //memset(runicast_entry,0, sizeof(Ru_msg_list));
    rimeaddr_copy(&runicast_entry->runicast_msg.last_hop,&rimeaddr_node_addr);
    runicast_entry->runicast_msg.native_sen_type = runicast_msg.native_sen_type;
    runicast_entry->runicast_msg.seqno = runicast_msg.seqno;
    rimeaddr_copy(&runicast_entry->runicast_msg.source,&runicast_msg.source);
    //  runicast_entry->runicast_msg.timestamp = runicast_msg.timestamp;
    runicast_entry->runicast_msg.value = runicast_msg.value;
    list_add(runicast_list,runicast_entry);


}

void pr_buf(){
    print_br_list();
    printf("-----------runicast list-------------\n");
    Ru_msg_list *ru_list;
    for(ru_list=list_head(runicast_list);ru_list!=NULL;ru_list=ru_list->next){
        printf("value %d | seq %d | hops %d | last hop %u.%u |\n",ru_list->runicast_msg.value,ru_list->runicast_msg.seqno,ru_list->runicast_msg.hops,ru_list->runicast_msg.last_hop.u8[0],ru_list->runicast_msg.last_hop.u8[1]);
    }
    printf("-----------routing table--------------\n");

    Rt_table* head;
    for(head=list_head(routing_table);head!=NULL;head=head->next){
        printf("| next hop %u.%u | total hops %d |\n",head->rt_entry.next_hop.u8[0],head->rt_entry.next_hop.u8[1],head->rt_entry.total_hops);
    }
    printf("-------------------------------\n");

}
/**
* initialize the local node
*/
void init_my_node(uint8_t native_sen_type) {
    rimeaddr_copy(&local_node.sid,&rimeaddr_node_addr);
    local_node.sen_type = native_sen_type;
    //local_node.head == NULL;
    local_node.frequency == MAX_BC_INTERVAL;
    local_node.subscribe = false;
    local_node.condition = NONE_CONDITION;
    //local_node.lamport_difference = 0;
    ru_send_success = false;
    printf("[set node address]->%d.%d\n",local_node.sid.u8[0],local_node.sid.u8[1]);
    if(local_node.sen_type == BASE_STATION){
        subscribe(SUBSCRIBE_INIT,FREQUENCY,CONDITION_DEFINE,ASSEMBLE_COUNT);
        Broadcast_msg *broadcast_msg;
        broadcast_msg = create_broadcast_msg(local_node.sen_type, NORMAL,MAX_BC_INTERVAL, NONE_TYPE
                , local_node.sid, local_node.sid, local_node.sid, 0, NONE_CONDITION,ASSEMBLE_COUNT,clock_seconds());
        check_update_rt_table(broadcast_msg);
    }
}

/**
 * subscribe node will assemble the data according to the conditions
 */
void assemble_data(){
    uint8_t value = random_rand()%10;
    assemble_total+=value;
    assemble_count++;
    if(value>max){
        max = value;
    }
    if(assemble_count>= local_node.assemble_count){
        Runicast_msg runicast_msg;
        runicast_msg.native_sen_type = local_node.sen_type;
        runicast_msg.seqno = clct_data_seqno;
        //runicast_msg.timestamp = CLOCK_SECOND;
        if(local_node.condition == MAX){
            runicast_msg.value = max;
        }else if(local_node.condition == AVERAGE ){
            runicast_msg.value = (assemble_total)/(assemble_count);
        }
        rimeaddr_copy(&runicast_msg.last_hop,&rimeaddr_node_addr);
        rimeaddr_copy(&runicast_msg.source,&rimeaddr_node_addr);
        //add_to_RU_list(runicast_msg);

        if(!runicast_is_transmitting(&runicast)){
            printf("[PUBLISH] publish data value->%u seqno %u\n",runicast_msg.value,runicast_msg.seqno);
            //single_publish(&runicast,runicast_msg);
            printf("[RU] timestamp %d seqno %u last hop address %u.%u\n", runicast_msg.timestamp,runicast_msg.seqno,runicast_msg.last_hop.u8[0],runicast_msg.last_hop.u8[1]);
            packetbuf_copyfrom(&runicast_msg, sizeof(Runicast_msg));
            runicast_send(&runicast,&find_best_route()->rt_entry.next_hop,MAX_RETRANSMISSIONS);
            packetbuf_clear();
        }


        if(clct_data_seqno>254){
            clct_data_seqno = 0;
        }
        clct_data_seqno++;
        assemble_count = 0;
        assemble_total = 0.0f;
        max = 0.0f;
    }
}

//


/***********************************************************************************/
static void
recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
    printf("[RECV] runicast\n");
    Runicast_msg *runicast_msg;
    runicast_msg = packetbuf_dataptr();
//
//    if(local_node.sen_type!=BASE_STATION){
    //add_to_RU_list(*runicast_msg);
//    Runicast_msg temp;
//    memcpy(&temp,runicast_msg, sizeof(Runicast_msg));
    // single_publish(&runicast,temp);
    packetbuf_copyfrom(runicast_msg, sizeof(Runicast_msg));
    printf("[RU] timestamp @%d seqno %u last hop address %u.%u\n", runicast_msg->timestamp,runicast_msg->seqno,runicast_msg->last_hop.u8[0],runicast_msg->last_hop.u8[1]);

    runicast_send(&runicast,&find_best_route()->rt_entry.next_hop,MAX_RETRANSMISSIONS);
    packetbuf_clear();
}

static void
broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from)
{
    Broadcast_msg *broadcast_msg;
    broadcast_msg = packetbuf_dataptr();
    //update_lamport_clock(broadcast_msg->lamport_clock);
    add_hops(broadcast_msg);
    if(broadcast_msg->msg_type == SUBSCRIBE||broadcast_msg->msg_type == UNSUBSCRIBE) {
        update_subscribtion(&local_node, broadcast_msg);
        add_broadcast_entry(broadcast_msg->native_sen_type,broadcast_msg->msg_type,broadcast_msg->frequency,
                            broadcast_msg->subscribe_type,*from,rimeaddr_node_addr,broadcast_msg->base_station,broadcast_msg->hops,CONDITION_DEFINE,0,clock_seconds());
    }else{
        check_update_rt_table(broadcast_msg);
    }

}

/***********************************************************************************/
static void
sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
    ru_send_success = true;
    printf("[FORWARD-SUCCESS]\n");
}
/*the function will be called when timeout*/
static void
timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
    ru_send_success = false;
    printf("[FORWARD-FAIL]\n");
    /**
     * remove invalid routing entry
     */
    remove_route_entry(current_route);
    printf("[REMOVE-ROUTING-ENTRY]->%u.%u\n",current_route->rt_entry.next_hop.u8[0],current_route->rt_entry.next_hop.u8[1]);

}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};  /**<recieve call back for broadcast*/

/***********************************************************************************/
PROCESS_THREAD(broadcast_process, ev, data)
{
    static struct etimer et; /*timer to wait*/
    if(!ifinit){
        list_init(broadcast_list);
        memb_init(&broadcast_list_memb);
        list_init(routing_table);
        memb_init(&routing_table_memb);
        init_my_node(NODE_INIT);
        ifinit = 1;
    }
    PROCESS_EXITHANDLER(broadcast_close(&broadcast);)
    PROCESS_BEGIN();
        printf("broadcasting....\n");
        broadcast_open(&broadcast, 229, &broadcast_call);
        while(1){
            etimer_set(&et,(random_rand()%MAX_BC_INTERVAL)*CLOCK_SECOND);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            /*add the best route entry to broadcast*/
            create_br_msg_from_RT_table();
            send_br_list(&broadcast);
            print_br_list();
            remove_all_br_entry();
        }
    PROCESS_END();
}
static const struct runicast_callbacks runicast_callbacks = {recv_runicast,
                                                             sent_runicast,
                                                             timedout_runicast};

PROCESS_THREAD(runicast_process, ev, data)
{
    PROCESS_EXITHANDLER(runicast_close(&runicast);)
    PROCESS_BEGIN();
        runicast_open(&runicast, 144, &runicast_callbacks);
        list_init(runicast_list);
        memb_init(&runicast_list_memb);
        while(1){
            int interval = RU_INTERVAL;
            if(local_node.subscribe == true){
                interval = local_node.frequency;
            }
            static struct etimer et; /*timer to wait*/
            etimer_set(&et,interval*CLOCK_SECOND);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            if(!runicast_is_transmitting(&runicast)){
                if(local_node.subscribe == true){
                    //publish(&runicast);
                    assemble_data(&runicast);
                }
            }
        }
    PROCESS_END();
}


//
PROCESS_THREAD(remove_stale_process, ev, data)
{
    static struct etimer et; /*timer to wait*/
    PROCESS_EXITHANDLER(runicast_close(&runicast);)
    PROCESS_BEGIN();
        while(1){
            etimer_set(&et,CHECK_STALE_PERIORD*CLOCK_SECOND);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            Rt_table *head;
            for(head=list_head(routing_table);head!=NULL;head=head->next){
                if(clock_seconds()-head->rt_entry.timestamp>STALE_PERIORD){
                    list_remove(routing_table,head);
                    memb_free(&routing_table_memb,head);
                }
            }
        }
    PROCESS_END();
}

PROCESS_THREAD(remove_br_process, ev, data)
{
    static struct etimer et; /*timer to wait*/
    PROCESS_EXITHANDLER(runicast_close(&runicast);)
    PROCESS_BEGIN();
        while(1){
            etimer_set(&et,BR_VALID_PERIORD*CLOCK_SECOND);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            remove_all_br_entry();
        }
    PROCESS_END();
}

PROCESS_THREAD(pr_buf_process, ev, data)
{
    PROCESS_BEGIN();
        // active = 0;
        SENSORS_ACTIVATE(button_sensor);
        while(1) {
            PROCESS_WAIT_EVENT_UNTIL(ev == sensors_event && data == &button_sensor);
            pr_buf();
        }
    PROCESS_END();
}