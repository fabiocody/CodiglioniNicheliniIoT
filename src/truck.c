#include "contiki.h"
#include "net/rime.h"
#include "list.h"
#include "memb.h"
#include "random.h"
#include "messages.h"
#include "config.h"
#include <stdio.h>
#include <math.h>

static unsigned char working = 0;
static unsigned char sending = 0;
static unsigned char x;
static unsigned char y;

static struct serving_bin {
    unsigned char id;
    unsigned char x;
    unsigned char y;
} serving_bin;


PROCESS(truck_proc, "Truck mote process");
AUTOSTART_PROCESSES(&truck_proc);


static void recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    printf("runicast message received from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);
    if (!working){
        working = 1;
        void *msg = packetbuf_dataptr();
        unsigned char msg_type = GET_MSG_TYPE(msg);
        switch (msg_type) {
            case ALERT_MSG:
                serving_bin.id = ((alert_msg_t *)msg)->id;
                serving_bin.x = ((alert_msg_t *)msg)->x;
                serving_bin.y = ((alert_msg_t *)msg)->y;
                sending = 1;
            case TRUCK_ACK:
                working = 0;
            default:
                break;
        }
    }    
}


static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("runicast message sent to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}


static void timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("runicast message timed out when sending to %d.%d, retransmissions %d\n", to->u8[0], to->u8[1], retransmissions);
}


// initialization runicast callbacks struct
static const struct runicast_callbacks runicast_callbacks = {recv_runicast,
                                                             sent_runicast,
                                                             timedout_runicast};
static struct runicast_conn runicast;
static const rimeaddr_t truck_addr = {{TRUCK_ADDR, 0}};


PROCESS_THREAD(truck_proc, ev, data){
    static struct etimer et;
    static struct etimer traveling_timer;
    static float travel_distance;
    static truck_msg_t msg = {TRUCK_MSG};
    PROCESS_EXITHANDLER(runicast_close(&runicast));
    PROCESS_BEGIN();
    rimeaddr_set_node_addr(&truck_addr);
    printf("MY ADDRESS IS %u.%u\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);
    runicast_open(&runicast, UNICAST_CHANNEL, &runicast_callbacks);
    x = random_rand() % 100;
    y = random_rand() % 100;
    while (1) {
        etimer_set(&et, CLOCK_SECOND*1);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        if (sending) {
            //travel_distance = ((serving_bin.x * serving_bin.x) + (serving_bin.y * serving_bin.y)); 
            travel_distance = 5;
            etimer_set(&traveling_timer, CLOCK_SECOND * travel_distance * BIN_TO_TRUCK_ALPHA);
            //while(runicast_is_transmitting(&runicast));
            rimeaddr_t recipient;
            packetbuf_copyfrom(&msg, sizeof(truck_msg_t));
            recipient.u8[0] = serving_bin.id;
            recipient.u8[1] = 0;
            printf("%u.%u: sending runicast to address %u.%u\n",
                rimeaddr_node_addr.u8[0],
                rimeaddr_node_addr.u8[1],
                recipient.u8[0],
                recipient.u8[1]);
            runicast_send(&runicast, &recipient, MAX_RETRANSMISSIONS);
            sending = 0;
        }
    }
    PROCESS_END();
}
