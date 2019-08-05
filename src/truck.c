#include "contiki.h"
#include "net/rime.h"
#include "list.h"
#include "memb.h"
#include "random.h"
#include "messages.h"
#include "config.h"
#include <stdio.h>
#include <math.h>

#define TRAVEL_EVENT 254

#define FALSE 0
#define TRUE  1

static unsigned char travelling = FALSE;
static unsigned char x;
static unsigned char y;

struct {
    unsigned short x;
    unsigned short y;
    unsigned char id;
} serving_bin;


PROCESS(truck_proc, "Truck process");
AUTOSTART_PROCESSES(&truck_proc);


static void handle_alert_msg(alert_msg_t *msg) {
    if (!travelling) {
        printf("traveling to %u\n", msg->id);
        travelling = TRUE;
        serving_bin.x = msg->x;
        serving_bin.y = msg->y;
        serving_bin.id = msg->id;
        process_post(&truck_proc, TRAVEL_EVENT, NULL);
    }
}


static void recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    //printf("runicast message received from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);
    void *msg = packetbuf_dataptr();
    unsigned char msg_type = GET_MSG_TYPE(msg);
    switch (msg_type) {
        case ALERT_MSG:
            puts("ALERT_MSG");
            handle_alert_msg((alert_msg_t *)msg);
            break;
        case TRUCK_ACK:
            puts("TRUCK_ACK");
            travelling = FALSE;
            break;
        default:
            break;
    }    
}


static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    //printf("runicast message sent to %u, retransmissions %d\n", to->u8[0], retransmissions);
}


static void timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("runicast message timed out when sending to %d, retransmissions %d\n", to->u8[0], retransmissions);
}


static const struct runicast_callbacks runicast_callbacks = {recv_runicast,
                                                             sent_runicast,
                                                             timedout_runicast};
static struct runicast_conn uc;
static rimeaddr_t truck_addr = {{TRUCK_ADDR, 0}};


PROCESS_THREAD(truck_proc, ev, data){
    static struct etimer travel_timer;
    static float travel_distance;
    static truck_msg_t msg = {TRUCK_MSG};
    PROCESS_EXITHANDLER(runicast_close(&uc));
    PROCESS_BEGIN();
    rimeaddr_set_node_addr(&truck_addr);
    printf("MY ADDRESS IS %u.%u\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
    runicast_open(&uc, UNICAST_CHANNEL, &runicast_callbacks);
    x = random_rand() % 100;
    y = random_rand() % 100;
    while (1) {
        PROCESS_WAIT_EVENT();
        if (ev == TRAVEL_EVENT) {
            travel_distance = 2;    // TODO
            etimer_set(&travel_timer, CLOCK_SECOND * travel_distance * BIN_TO_TRUCK_ALPHA);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&travel_timer));
            x = serving_bin.x;
            y = serving_bin.y;
            rimeaddr_t recipient = {{serving_bin.id, 0}};
            packetbuf_copyfrom(&msg, sizeof(truck_msg_t));
            while (runicast_is_transmitting(&uc));
            runicast_send(&uc, &recipient, MAX_RETRANSMISSIONS);
        }
    }
    PROCESS_END();
}
