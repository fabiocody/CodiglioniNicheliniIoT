// test.c

#include "contiki.h"
#include "net/rime.h"
#include "random.h"
#include "messages.h"
#include "config.h"
#include <stdio.h>


#define MAX_NEIGHBORS 8
#define MAX_GEN_TRASH 10

#define ALERT_EVENT 255
#define FULL_EVENT 254

#define FALSE 0
#define TRUE  1


PROCESS(trash_proc, "Trash generation process");
PROCESS(alert_mode_proc, "Alert mode process");
PROCESS(full_mode_proc, "Full bin mode process");

AUTOSTART_PROCESSES(&trash_proc, &alert_mode_proc, &full_mode_proc);


//static struct broadcast_conn bc;  // TODO
static struct runicast_conn uc;
static unsigned char trash = 0;
static unsigned char x = 0;
static unsigned char y = 0;
static unsigned char alert_mode = FALSE;
static const rimeaddr_t truck_addr = {{TRUCK_ADDR, 0}};


static void handle_move_msg() {
    // TODO
}


static void broadcast_recv(struct broadcast_conn *c, const rimeaddr_t *from) {
    void *msg = packetbuf_dataptr();
    unsigned char msg_type = GET_MSG_TYPE(msg);
    if (msg_type == MOVE_MSG) {
        puts("MOVE_MSG");
        handle_move_msg();
    } else printf("ERROR: undefined broadcast message received from %u\n", from->u8[0]);
}


static void handle_truck_msg() {
    static truck_ack_t ack = {TRUCK_ACK};
    alert_mode = FALSE;
    puts("emptying trash");
    trash = 0;
    packetbuf_copyfrom(&ack, sizeof(truck_ack_t));
    while (runicast_is_transmitting(&uc));
    runicast_send(&uc, &truck_addr, MAX_RETRANSMISSIONS);
}


static void handle_move_reply() {
    // TODO
}


static void handle_trash_msg() {
    // TODO
}


static void unicast_recv(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    void *msg = packetbuf_dataptr();
    unsigned char msg_type = GET_MSG_TYPE(msg);
    switch (msg_type) {
        case TRUCK_MSG:
            puts("TRUCK_MSG");
            handle_truck_msg();
            break;
        case MOVE_REPLY:
            puts("MOVE_REPLY");
            handle_move_reply();
            break;
        case TRASH_MSG:
            puts("TRASH_MSG");
            handle_trash_msg();
            break;
        default:
            printf("ERROR: undefined unicast message received from %u\n", from->u8[0]);
    }
}


static void unicast_sent(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    //printf("runicast message sent to %u, retransmissions %u\n", to->u8[0], retransmissions);
}


static void unicast_timedout(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("runicast message timed out when sending to %u, retransmissions %u\n", to->u8[0], retransmissions);
}


static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static const struct runicast_callbacks unicast_call = {unicast_recv, unicast_sent, unicast_timedout};


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
        printf("TRASH = %u (GEN_TRASH = %u)\n", trash + gen_trash, gen_trash);
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
    PROCESS_EXITHANDLER(runicast_close(&uc));
    PROCESS_BEGIN();
    msg.x = x;
    msg.y = y;
    msg.id = rimeaddr_node_addr.u8[0];
    runicast_open(&uc, UNICAST_CHANNEL, &unicast_call);
    while (1) {
        PROCESS_WAIT_EVENT();
        if (ev == ALERT_EVENT) {
            while (alert_mode) {
                packetbuf_copyfrom(&msg, sizeof(alert_msg_t));
                while (runicast_is_transmitting(&uc));
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
    //static move_msg_t msg;    // TODO
    PROCESS_BEGIN();
    while(1) {
        PROCESS_WAIT_EVENT();
        /*if (ev == FULL_EVENT) {   // TODO
            while(1){
            }
        }*/
    }
    PROCESS_END();
}

