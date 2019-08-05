// test.c

#include "contiki.h"
#include "net/rime.h"
#include "list.h"
#include "memb.h"
#include "random.h"
#include "messages.h"
#include "config.h"
#include <stdio.h>


#define MAX_NEIGHBORS 8
#define MAX_GEN_TRASH 10

#define ALERT_EVENT 254
#define FULL_EVENT 255

#define FULL_THRESHOLD 100
#define ALERT_THRESHOLD FULL_THRESHOLD - 15

#define FALSE 0
#define TRUE  1


typedef struct neighbor {
    struct neighbor *next;
    rimeaddr_t addr;
} neighbor_t;


MEMB(neighbors_memb, neighbor_t, MAX_NEIGHBORS);
LIST(neighbors_list);


PROCESS(announce_proc, "Announce process");
PROCESS(debug_print_proc, "DEBUG Print process");
PROCESS(trash_proc, "Trash generation process");
PROCESS(alert_mode_proc, "Alert mode process");
PROCESS(full_mode_proc, "Full bin mode process");

AUTOSTART_PROCESSES(&announce_proc, &trash_proc, &alert_mode_proc, &full_mode_proc);


static struct broadcast_conn bc;
static unsigned char trash = 0;
static unsigned char x = 0;
static unsigned char y = 0;
static unsigned char alert_mode = FALSE;


static void handle_announce_msg(const rimeaddr_t *from) {
    neighbor_t *n;
    // check if recv address is already in the list
    for (n = list_head(neighbors_list); n != NULL; n = list_item_next(n))
        if (rimeaddr_cmp(&n->addr, from)) break;
    // add recv addr if not in the list
    if (n == NULL) {
        n = memb_alloc(&neighbors_memb);
        if (n == NULL) return;
        rimeaddr_copy(&n->addr, from);
        list_add(neighbors_list, n);
    }
    packetbuf_clear();
}


static void handle_alert_msg(alert_msg_t *msg) {
    printf("ALERT MSG from node %u at %u;%u\n", msg->id, msg->x, msg->y);
    packetbuf_clear();
}


static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
    void *msg = packetbuf_dataptr();
    unsigned char msg_type = GET_MSG_TYPE(msg);
    switch (msg_type) {
        case ANNOUNCE_MSG:
            handle_announce_msg(from);
            break;
        case ALERT_MSG:
            handle_alert_msg((alert_msg_t *)msg);
            break;
        case MOVE_MSG:
            break;
    }
}


static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static const struct runicast_callbacks runicast_call = {};


// NEIGHBOUR DISCOVERY PROCESS
PROCESS_THREAD(announce_proc, ev, data) {
    static struct etimer et;
    static announce_msg_t msg = {ANNOUNCE_MSG};
    PROCESS_EXITHANDLER(broadcast_close(&bc));
    PROCESS_BEGIN();
    broadcast_open(&bc, BROADCAST_CHANNEL, &broadcast_call);
    while (1) {
        etimer_set(&et, CLOCK_SECOND * (10 + random_rand() % 21));
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        packetbuf_copyfrom(&msg, sizeof(announce_msg_t));
        broadcast_send(&bc);
    }
    PROCESS_END();
}


// TRASH GENERATION PROCESS
PROCESS_THREAD(trash_proc, ev, data) {
    static struct etimer et;
    static unsigned char gen_trash = 0;
    PROCESS_BEGIN();
    x = random_rand() % 100;    // x coordinate random initialization
    y = random_rand() % 100;    // y coordinate random initiliazation
    while (1) {
        etimer_set(&et, CLOCK_SECOND * (1 + random_rand() % 30));   // set timer with random value
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        gen_trash = 1 + random_rand() % MAX_GEN_TRASH;  // trash generation
        printf("GEN_TRASH = %u\n", gen_trash);
        printf("TRASH = %u\n", trash + gen_trash);
        if (trash + gen_trash < FULL_THRESHOLD) {
            trash += gen_trash; 
            if (trash >= ALERT_THRESHOLD) {
                alert_mode = TRUE;
                process_post(&alert_mode_proc, ALERT_EVENT, NULL);
            }
        } else {
            process_post(&full_mode_proc, FULL_EVENT, NULL);
        }    
    }
    PROCESS_END();
}


// ALERT MODE PROCESS
PROCESS_THREAD(alert_mode_proc, ev, data) {
    static struct etimer et;
    static alert_msg_t msg = {ALERT_MSG};
    msg.x = x;
    msg.y = y;
    msg.id = rimeaddr_node_addr.u8[0];
    PROCESS_BEGIN();
    while (1) {
        PROCESS_WAIT_EVENT();
        if (ev == ALERT_EVENT) 
            while (alert_mode) {
                packetbuf_copyfrom(&msg, sizeof(alert_msg_t));
                broadcast_send(&bc);
                etimer_set(&et, CLOCK_SECOND * 5);
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
            }
    }
    PROCESS_END();
}


// FULL MODE PROCESS
PROCESS_THREAD(full_mode_proc, ev, data) {
    static move_msg_t msg;
    PROCESS_BEGIN();
    while(1) {
        PROCESS_WAIT_EVENT();
        /*if (ev == FULL_EVENT) { 
            while(1){
            }
        }*/
    }
    PROCESS_END();
}


// NEIGHBOR DEBUGGING PROCESS
PROCESS_THREAD(debug_print_proc, ev, data) {
    static struct etimer et;
    PROCESS_BEGIN();
    while (1) {
        etimer_set(&et, CLOCK_SECOND * 30);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        neighbor_t *n;
        for (n = list_head(neighbors_list); n != NULL; n = list_item_next(n))
            printf("%u ", n->addr.u8[0]);
        puts("");
    }
    PROCESS_END();
}

