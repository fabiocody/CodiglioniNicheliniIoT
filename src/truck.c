// truck.c

#include "contiki.h"
#include "net/rime.h"
#include "list.h"
#include "memb.h"
#include "random.h"
#include "messages.h"
#include "config.h"
#include "toolkit.h"
#include <stdio.h>
#include <math.h>

#define TRAVEL_EVENT 254

#define FALSE 0
#define TRUE  1


static unsigned char traveling = FALSE;
static unsigned char x;
static unsigned char y;
static unsigned char history_table[MAX_NODES];

static struct {
    unsigned char x;
    unsigned char y;
    unsigned char id;
} serving_bin;


PROCESS(truck_proc, "Truck process");
AUTOSTART_PROCESSES(&truck_proc);


/*
 * Function: recvunicast
 * This function hadles all callback related to te receving of a unicast message
 */
static void recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno) {
    unsigned char from_addr = from->u8[0];
    if (history_table[from_addr] != seqno) {
        history_table[from_addr] = seqno;
        //printf("runicast message received from %d.%d, seqno %d\n", from->u8[0], from->u8[1], seqno);
        void *msg = packetbuf_dataptr();
        unsigned char msg_type = GET_MSG_TYPE(msg);
        if (msg_type == ALERT_MSG) {
            puts("incoming message: ALERT_MSG");
            if (!traveling) {
                alert_msg_t alert_msg = *(alert_msg_t *)msg;
                printf("traveling to %u\n", alert_msg.id);
                traveling = TRUE;
                serving_bin.x = alert_msg.x;
                serving_bin.y = alert_msg.y;
                serving_bin.id = alert_msg.id;
                process_post(&truck_proc, TRAVEL_EVENT, NULL);
            }
        } else {
            printf("ERROR: unrecognized unicast message of type %u received from %u\n", msg_type, from->u8[0]);
        }
    }
}


/*
 * Sent unicast message callback.
 * Just print to console
 */
static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    //printf("runicast message sent to %u, retransmissions %d\n", to->u8[0], retransmissions);
}


/*
 * Timeout unicast message callback.
 * Just print to console
 */
static void timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions) {
    printf("ERROR: runicast message timed out when sending to %d, retransmissions %d\n", to->u8[0], retransmissions);
}


static const struct runicast_callbacks runicast_callbacks = {recv_runicast,
                                                             sent_runicast,
                                                             timedout_runicast};
static struct runicast_conn uc;
//static rimeaddr_t truck_addr = {{TRUCK_ADDR, 0}};


/* PROCESS: truck_proc
 * This is the main process of the truck's firmware.
 * It wait for the callback to raise an event, then distance is calculated, travel time
 * is simulated, and message is sent
 */
PROCESS_THREAD(truck_proc, ev, data){
    static struct etimer travel_timer;
    static struct etimer busy_timer;
    static truck_msg_t msg = {TRUCK_MSG};
    static rimeaddr_t recipient = {{0, 0}};
    PROCESS_EXITHANDLER(runicast_close(&uc));

    PROCESS_BEGIN();
    //rimeaddr_set_node_addr(&truck_addr);
    //printf("MY ADDRESS IS %u.%u\n", rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1]);

    unsigned short i;
    for (i = 0; i < MAX_NODES; i++)
        history_table[i] = -1;
    runicast_open(&uc, UNICAST_CHANNEL, &runicast_callbacks);
    x = random_rand() % MAX_COORDINATE;
    y = random_rand() % MAX_COORDINATE;

    while (1) {
        PROCESS_WAIT_EVENT();
        if (ev == TRAVEL_EVENT) {
            // distance calculation
            unsigned int d = distance(x, y, serving_bin.x, serving_bin.y);
            d *= BIN_TO_TRUCK_ALPHA;
            printf("travel_time = %u\n", d);

            // travel time is simulated with a timer
            etimer_set(&travel_timer, CLOCK_SECOND * d);
            PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&travel_timer));

            // message sending
            x = serving_bin.x;
            y = serving_bin.y;
            recipient.u8[0] = serving_bin.id;
            while (runicast_is_transmitting(&uc)) {
                etimer_set(&busy_timer, CLOCK_SECOND / BUSY_TIMER_DIVIDER);
                PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&busy_timer));
            }
            packetbuf_copyfrom(&msg, sizeof(truck_msg_t));
            runicast_send(&uc, &recipient, MAX_RETRANSMISSIONS);
            traveling = FALSE;
            printf("arrived to node %d\n", serving_bin.id);
        }
    }
    PROCESS_END();
}
