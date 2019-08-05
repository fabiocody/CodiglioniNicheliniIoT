#include "contiki.h"
#include "net/rime.h"
#include "list.h"
#include "memb.h"
#include "random.h"
#include "messages.h"
#include "config.h"
#include <stdio.h>
#include <math.h>

unsigned char working = 0;
unsigned char sending = 0;
unsigned char x;
unsigned char y;

static struct serving_bin {
    unsigned char id;
    unsigned char x;
    unsigned char y;
} serving_bin;

/*---------------------------------------------------------------------------*/
PROCESS(truck_process, "Truck mote process");
AUTOSTART_PROCESSES(&test_runicast_process);

/*---------------------------------------------------------------------------*/
static void recv_runicast(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno)
{
   printf("runicast message received from %d.%d, seqno %d\n",
         from->u8[0], from->u8[1], seqno);
   
   if (!working){
       working = 1;
       void *msg = packetbuf_dataptr();
       unsigned char msg_type = GET_MSG_TYPE(msg);
       switch (msg_type) {
           case ALERT_MSG:
               serving_bin.id = ((alert_msg_t *)msg.id);
               serving_bin.x = ((alert_msg_t *)msg.x);
               serving_bin.y = ((alert_msg_t *)msg.y);
               sending = 1;
           case ACK_MSG:
               working = 0;
           default:
               break;
       }
   }    
}

static void sent_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
  printf("runicast message sent to %d.%d, retransmissions %d\n",
         to->u8[0], to->u8[1], retransmissions);
}

static void timedout_runicast(struct runicast_conn *c, const rimeaddr_t *to, uint8_t retransmissions)
{
  printf("runicast message timed out when sending to %d.%d, retransmissions %d\n",
         to->u8[0], to->u8[1], retransmissions);
}

// initialization runicast callbacks struct
static const struct runicast_callbacks runicast_callbacks = {recv_runicast,
                                                             sent_runicast,
                                                             timedout_runicast};
static struct runicast_conn runicast;
/*---------------------------------------------------------------------------*/

PROCESS_THREAD(truck_process, ev, data){
    static struct etimer et;
    static struct etimer traveling_timer;
    static float travel_distance;

    PROCESS_EXITHANDLER(runicast_close(&runicast));
    
    PROCESS_BEGIN();
    runicast_open(&runicast, UNICAST_CHANNEL, &runicast_callbacks);
    x = random_rand() % 100;
    y = random_rand() % 100;

    while (1) {
        etimer_set(&et, CLOCK_SECOND*1);
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        if (sending) {
            travel_distance = sqrt(pow(serving_bin.x,2)+pow(serving_bin.y,2)); 
            etimer_set(&traveling_time, CLOCK_SECOND*traveling_time*BIN_TO_TRUCK_ALPHA)
            while(runicast_is_transmitting(&runicast)) {;}
            rimeaddr_t recipient;
            recipient.u8[0] = serving_bin.id;
            recipient.u8[1] = 0;

            printf("%u.%u: sending runicast to address %u.%u\n",
             rimeaddr_node_addr.u8[0],
             rimeaddr_node_addr.u8[1],
             recv.u8[0],
             recv.u8[1]);

            runicast_send(&runicast, &recv, MAX_RETRANSMISSIONS);
            sending = 0;
        }
    }
    PROCESS_END();
}
