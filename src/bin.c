// bin.c

#include "contiki.h"
#include "net/rime.h"
#include "mt.h"
#include "random.h"
#include "messages.h"
#include "config.h"
#include "toolkit.h"
#include <stdio.h>


#define MAX_NEIGHBORS 8
#define MAX_GEN_TRASH 10

#define ALERT_EVENT 255
#define FULL_EVENT 254
#define RESPONSE_TRUCK_MSG_EVENT 253
#define RESPONSE_MOVE_MSG_EVENT 252
#define MAX_NEIGHBORS 8
#define FALSE 0
#define TRUE  1


PROCESS(trash_proc, "Trash generation process");
PROCESS(alert_mode_proc, "Alert mode process");
PROCESS(full_mode_proc, "Full bin mode process");
PROCESS(responses_proc, "Process to handle direct responses");

AUTOSTART_PROCESSES(&trash_proc, &alert_mode_proc, &full_mode_proc, &responses_proc);

static struct broadcast_conn bc;
static struct runicast_conn uc;
static unsigned char trash = 0;
static unsigned char gen_trash = 0;
static unsigned char x = 0;
static unsigned char y = 0;
static unsigned char alert_mode = FALSE;
static const rimeaddr_t truck_addr = {{TRUCK_ADDR, 0}};
static unsigned char history_table[MAX_NODES];

typedef struct neighbor{
    struct neighbor *next;
    rimeaddr_t addr;
    unsigned char x;
    unsigned char y;
    unsigned int distance;
} neighbor_t;


MEMB(neighbors_memb, neighbor_t, MAX_NEIGHBORS);
LIST(neighbor_list);

static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
    void *msg = packetbuf_dataptr();
    unsigned char msg_type = GET_MSG_TYPE(msg);
    if (msg_type == MOVE_MSG) {
        puts("MOVE_MSG");
        if (trash < ALERT_THRESHOLD) process_post(&responses_proc, RESPONSE_MOVE_MSG_EVENT, from);
    } else printf("ERROR: unrecognized broadcast message of type %u received from %u\n", msg_type, from->u8[0]);
}


static void unicast_recv(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    unsigned char from_addr = from->u8[0];
    if (history_table[from_addr] != seqno) {
        history_table[from_addr] = seqno;
        printf("runicast message received from %u, seqno %u\n", from->u8[0], seqno);
        void *msg = packetbuf_dataptr();
        unsigned char msg_type = GET_MSG_TYPE(msg);
        if (msg_type == TRUCK_MSG) {
            puts("TRUCK_MSG");
            alert_mode = FALSE;
            puts("emptying trash");
            trash = 0;
            process_post(&responses_proc, RESPONSE_TRUCK_MSG_EVENT, NULL);
        } else if (msg_type == MOVE_REPLY) {
            puts("MOVE_REPLY");
            move_reply_t *reply = (move_reply_t*)msg;
            neighbor_t *tmp = memb_alloc(&neighbors_memb);
            if (n == NULL) return;
            tmp->next = NULL;
            tmp->addr = *from;
            tmp->x = msg->x;
            tmp->y = msg->y;
            tmp->distance = 0;
            list_add(neighbor_list, tmp);
        } else if (msg_type == TRASH_MSG) {
            puts("TRASH_MSG");
            trash_msg_t trash_msg = (trash_msg_t *)msg;
            trash += trash_msg->trash;
        } else {
            printf("ERROR: unrecognized unicast message of type %u received from %u\n", msg_type, from->u8[0]);
        }
    }
}


static void unicast_sent(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("runicast message sent to %u, retransmissions %u\n", to->u8[0], retransmissions);
}


static void unicast_timedout(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("runicast message timed out when sending to %u, retransmissions %u\n", to->u8[0], retransmissions);
}


static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static const struct runicast_callbacks unicast_call = {unicast_recv, unicast_sent, unicast_timedout};


// TRASH GENERATION PROCESS
PROCESS_THREAD(trash_proc, ev, data) {
    static struct etimer et;
    PROCESS_BEGIN();
    x = random_rand() % MAX_COORDINATE;    // x coordinate random initialization
    y = random_rand() % MAX_COORDINATE;    // y coordinate random initiliazation
    while (1) {
        etimer_set(&et, CLOCK_SECOND * (1 + random_rand() % 30));   // set timer with random value
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        gen_trash = 1 + random_rand() % MAX_GEN_TRASH;  // trash generation
        printf("TRASH = %u (GEN_TRASH = %u)\n", trash + gen_trash, gen_trash);
        if (trash + gen_trash < FULL_THRESHOLD) {
            trash += gen_trash; 
            if (trash >= ALERT_THRESHOLD) {
                alert_mode = TRUE;
                process_post(&alert_mode_proc, ALERT_EVENT, NULL);
            }
        } else {
            process_post(&full_mode_proc, FULL_EVENT, NULL); // set bin to FULL MODE
        }    
    }
    PROCESS_END();
}


// ALERT MODE PROCESS
PROCESS_THREAD(alert_mode_proc, ev, data) {
    static struct etimer et;
    static struct etimer busy_timer;
    static alert_msg_t msg = {ALERT_MSG};
    PROCESS_EXITHANDLER(runicast_close(&uc));
    PROCESS_BEGIN();
    msg.x = x;
    msg.y = y;
    msg.id = rimeaddr_node_addr.u8[0];
    unsigned short i;
    for (i = 0; i < MAX_NODES; i++)
        history_table[i] = -1;
    runicast_open(&uc, UNICAST_CHANNEL, &unicast_call);
    while (1) {
        PROCESS_WAIT_EVENT();
        if (ev == ALERT_EVENT) {
            while (alert_mode) {
                packetbuf_copyfrom(&msg, sizeof(alert_msg_t));
                while (runicast_is_transmitting(&uc)) {
                    etimer_set(&busy_timer, CLOCK_SECOND / BUSY_TIMER_DIVIDER);
                    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&busy_timer));
                }
                runicast_send(&uc, &truck_addr, MAX_RETRANSMISSIONS);
                etimer_set(&et, CLOCK_SECOND * 5);
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            }
        }
    }
    PROCESS_END();
}


// FULL MODE PROCESS
PROCESS_THREAD(full_mode_proc, ev, data) {
    static move_msg_t msg = {MOVE_MSG};
    static struct etimer et;
    static struct etimer busy_timer;
    PROCESS_EXITHANDLER(broadcast_close(&bc));
    PROCESS_BEGIN();
    broadcast_open(&bc, BROADCAST_CHANNEL, %broadcast_call);
    while(1) {
        PROCESS_WAIT_EVENT();
        if (ev == FULL_EVENT){
            puts("FULL PROCESS");
            packetbuf_copyfrom(&msg, sizeof(move_msg_t));
            
            while (list_head(neighbor_list)){ // neighbor_list cleaning
                list_pop(neighbor_list);
            }
            
            broadcast_send(&bc);
            etimer_set(&et, CLOCK_SECOND * 2);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            
            neighbor_t *ptr; // compute distance for each node which responded
            neighbor_t *min;
            for(ptr = list_head(ptr), min = ptr; ptr; ptr = list_item_next(ptr)){
                ptr->distance = distance(x, y, ptr->x, ptr->y);
                if (ptr->distance < min->distance) min = ptr;  
            }

            if (min = NULL) puts("ERROR: no replies");
            else {
                tras_msg_t t_msg = {TRASH_MSG, gen_trash};
                packetbuf_copyfrom(&t_msg, sizeof(t_msg));
                while (runicast_is_transmitting(&uc)) {
                    etimer_set(&busy_timer, CLOCK_SECOND / BUSY_TIMER_DIVIDER);
                    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&busy_timer));
                }
                runicast_send(&uc, &(min->addr), MAX_RETRANSMISSIONS);

            }
        }
    }
    PROCESS_END();
}


// RESPONSES PROCESS -- Process to handle responses to other nodes' requests
PROCESS_THREAD(responses_proc, ev, data) {
    static struct etimer busy_timer;
    static truck_ack_t truck_ack = {TRUCK_ACK};
    static move_reply_t mv_reply = {MOVE_REPLY, x, y};
    PROCESS_BEGIN();
    while (1) {
        PROCESS_WAIT_EVENT();
        if (ev == RESPONSE_TRUCK_MSG_EVENT) { // handle responses to truck msg
            while (runicast_is_transmitting(&uc)) {
                etimer_set(&busy_timer, CLOCK_SECOND / BUSY_TIMER_DIVIDER);
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&busy_timer));
            }
            packetbuf_copyfrom(&truck_ack, sizeof(truck_ack_t));
            runicast_send(&uc, &truck_addr, MAX_RETRANSMISSIONS);

        } else if (ev == RESPONSE_MOVE_MSG_EVENT) { // handle responses to move msg
            while (runicast_is_transmitting(&uc)) {
                etimer_set(&busy_timer, CLOCK_SECOND / BUSY_TIMER_DIVIDER);
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&busy_timer));
            }
            packetbuf_dataptr(mv_reply, sizeof(move_reply_t));
            runicast_send(&uc, (rimeaddr_t *)data, MAX_RETRANSMISSIONS);
        }
    }
    PROCESS_END();
}

