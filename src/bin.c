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

#define ALERT_EVENT 255
#define FULL_EVENT 254
#define RESPONSE_TRUCK_MSG_EVENT 253
#define RESPONSE_MOVE_MSG_EVENT 252

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
static unsigned char generation_mode = TRUE;
static rimeaddr_t truck_addr = {{TRUCK_ADDR, 0}};
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


/**
 * Custom callback for broadcast received messages. 
 * When a MOVE_MSG is received, it triggers a process to handle it. 
 */
static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
    void *msg = packetbuf_dataptr();
    unsigned char msg_type = GET_MSG_TYPE(msg);
    if (msg_type == MOVE_MSG) {
        puts("incoming message: MOVE_MSG");
        if (trash < ALERT_THRESHOLD)
            process_post(&responses_proc, RESPONSE_MOVE_MSG_EVENT, msg);
    } else printf("ERROR: unrecognized broadcast message of type %u received from %u\n", msg_type, from->u8[0]);
}


/**
 * Custom callback for unicast received messages. Messages are filtered by the msg_type field in msg struct. 
 * Each of them is handled in a different way.
 */
static void unicast_recv(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    unsigned char from_addr = from->u8[0];
    if (history_table[from_addr] != seqno) {
        history_table[from_addr] = seqno;
        //printf("runicast message received from %u, seqno %u\n", from->u8[0], seqno);
        void *msg = packetbuf_dataptr();
        unsigned char msg_type = GET_MSG_TYPE(msg);
        if (msg_type == TRUCK_MSG) { // reset trash qty and disable alert_mode
            puts("incoming message: TRUCK_MSG");
            alert_mode = FALSE;
            puts("trash = 0");
            trash = 0;
            process_post(&responses_proc, RESPONSE_TRUCK_MSG_EVENT, NULL);
        } else if (msg_type == MOVE_REPLY) { // allocate a list for neighbors and initialize struct
            puts("incoming message: MOVE_REPLY");
            move_reply_t *reply = (move_reply_t*)msg;
            neighbor_t *tmp = memb_alloc(&neighbors_memb);
            if (tmp == NULL) return;
            tmp->next = NULL;
            tmp->addr = *from;
            tmp->x = reply->x;
            tmp->y = reply->y;
            tmp->distance = 0;
            list_add(neighbor_list, tmp);
        } else if (msg_type == TRASH_MSG) { // update le
            puts("incoming message: TRASH_MSG");
            trash_msg_t *trash_msg = (trash_msg_t *)msg;
            trash += trash_msg->trash;
            printf("trash = %u\n", trash);
        } else {
            printf("ERROR: unrecognized unicast message of type %u received from %u\n", msg_type, from->u8[0]);
        }
    }
}


/**
 * Standard callback for unicast sent
 */
static void unicast_sent(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    //printf("runicast message sent to %u, retransmissions %u\n", to->u8[0], retransmissions);
}


/**
 * Standard callback for timed-out unicast message
 */
static void unicast_timedout(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("ERROR: runicast message timed out when sending to %u, retransmissions %u\n", to->u8[0], retransmissions);
}


// Callbacks registration
static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static const struct runicast_callbacks unicast_call = {unicast_recv, unicast_sent, unicast_timedout};


/**
 *  TRASH GENERATION PROCESS.   
 */
PROCESS_THREAD(trash_proc, ev, data) {
    static struct etimer et;    // initialize recursive timer
    PROCESS_BEGIN();
    x = random_rand() % MAX_COORDINATE;    // x coordinate random initialization
    y = random_rand() % MAX_COORDINATE;    // y coordinate random initiliazation
    puts("trash = 0");
    while (1) {
        etimer_set(&et, CLOCK_SECOND * (1 + random_rand() % 30));   // set timer with random value
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        if (generation_mode) {
            gen_trash = 1 + random_rand() % MAX_GEN_TRASH;  // trash generation
            if (trash + gen_trash < FULL_THRESHOLD) {
                trash += gen_trash;
                if (trash >= ALERT_THRESHOLD) {
                    alert_mode = TRUE;
                    process_post(&alert_mode_proc, ALERT_EVENT, NULL);      // wake up alert mode process
                }
            } else {
                process_post(&full_mode_proc, FULL_EVENT, NULL);    // wake up full mode process
            }
            printf("trash = %u (gen_trash = %u)\n", trash, gen_trash);
        }
    }
    PROCESS_END();
}


/**
 *  ALERT MODE PROCESS
 **/
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
                while (runicast_is_transmitting(&uc)) {
                    etimer_set(&busy_timer, CLOCK_SECOND / BUSY_TIMER_DIVIDER);
                    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&busy_timer));
                }
                packetbuf_copyfrom(&msg, sizeof(alert_msg_t));
                runicast_send(&uc, &truck_addr, MAX_RETRANSMISSIONS);
                puts("ALERT message sent");

                // Set timer for periodic retransmission
                etimer_set(&et, CLOCK_SECOND * ALERT_PERIOD);
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            }
        }
    }
    PROCESS_END();
}


/**
 *  FULL MODE PROCESS.
 **/ 
PROCESS_THREAD(full_mode_proc, ev, data) {
    static move_msg_t move_msg = {MOVE_MSG};
    static trash_msg_t trash_msg = {TRASH_MSG};
    static struct etimer et;
    PROCESS_EXITHANDLER(broadcast_close(&bc));
    PROCESS_BEGIN();
    broadcast_open(&bc, BROADCAST_CHANNEL, &broadcast_call);
    move_msg.x = x;
    move_msg.y = y;
    move_msg.id = rimeaddr_node_addr.u8[0];
    while(1) {
        PROCESS_WAIT_EVENT();
        if (ev == FULL_EVENT){
            generation_mode = FALSE;
            puts("entering neighbor mode");

            while (list_head(neighbor_list))     // neighbor_list cleaning
                list_pop(neighbor_list);

            packetbuf_copyfrom(&move_msg, sizeof(move_msg_t));
            broadcast_send(&bc);
            etimer_set(&et, CLOCK_SECOND * 2);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

            static neighbor_t *ptr;     // compute distance for each node which responded
            static neighbor_t *min = NULL;
            for (ptr = list_head(neighbor_list), min = ptr; ptr; ptr = list_item_next(ptr)){
                ptr->distance = distance(x, y, ptr->x, ptr->y);
                if (ptr->distance < min->distance) min = ptr;
            }

            if (min == NULL) { 
                puts("WARNING: no replies");
            } else {
                printf("sending trash to %u\n", min->addr.u8[0]);
                trash_msg.trash = gen_trash;
                while (runicast_is_transmitting(&uc)) {
                    etimer_set(&et, CLOCK_SECOND / BUSY_TIMER_DIVIDER);
                    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
                }
                packetbuf_copyfrom(&trash_msg, sizeof(trash_msg_t));
                runicast_send(&uc, &(min->addr), MAX_RETRANSMISSIONS);
            }
            generation_mode = TRUE;
        }
    }
    PROCESS_END();
}


/**
 *  RESPONSES PROCESS -- Process to handle responses to other nodes' requests.
 **/ 
PROCESS_THREAD(responses_proc, ev, data) {
    static struct etimer et;
    static move_msg_t move_msg;
    static move_reply_t move_reply = {MOVE_REPLY};
    PROCESS_BEGIN();
    move_reply.x = x;
    move_reply.y = y;
    while (1) {
        PROCESS_WAIT_EVENT();
        if (ev == RESPONSE_MOVE_MSG_EVENT) {    // handle responses to move msg
            move_msg = *(move_msg_t *)data;
            int d = distance(x, y, move_msg.x, move_msg.y);
            etimer_set(&et, CLOCK_SECOND * d * BIN_TO_BIN_ALPHA);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            while (runicast_is_transmitting(&uc)) {
                etimer_set(&et, CLOCK_SECOND / BUSY_TIMER_DIVIDER);
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            }
            packetbuf_copyfrom(&move_reply, sizeof(move_reply_t));
            rimeaddr_t addr = {{move_msg.id, 0}};
            runicast_send(&uc, &addr, MAX_RETRANSMISSIONS);
        }
    }
    PROCESS_END();
}
