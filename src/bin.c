// test.c

#include "contiki.h"
#include "net/rime.h"
#include "list.h"
#include "memb.h"
#include "random.h"
#include "messages.h"
#include <stdio.h>


typedef struct neighbor {
    struct neighbor *next;
    rimeaddr_t addr;
} neighbor_t;


#define MAX_NEIGHBORS 8
MEMB(neighbors_memb, neighbor_t, MAX_NEIGHBORS);
LIST(neighbors_list);


PROCESS(announce_proc, "Announce process");
PROCESS(debug_print_proc, "DEBUG Print process");
AUTOSTART_PROCESSES(&announce_proc, &debug_print_proc);


static void handle_announce_msg(const rimeaddr_t *from) {
    neighbor_t *n;
    for (n = list_head(neighbors_list); n != NULL; n = list_item_next(n))
        if (rimeaddr_cmp(&n->addr, from)) break;
    if (n == NULL) {
        n = memb_alloc(&neighbors_memb);
        if (n == NULL) return;
        rimeaddr_copy(&n->addr, from);
        list_add(neighbors_list, n);
    }
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
            break;
        case MOVE_MSG:
            break;
    }
}

static const struct broadcast_callbacks broadcast_call = {broadcast_recv};
static struct broadcast_conn broadcast;


PROCESS_THREAD(announce_proc, ev, data) {
    static struct etimer et;
    static announce_msg_t msg;
    PROCESS_EXITHANDLER(broadcast_close(&broadcast));
    PROCESS_BEGIN();
    broadcast_open(&broadcast, 129, &broadcast_call);
    while (1) {
        etimer_set(&et, CLOCK_SECOND * (10 + random_rand() % 21));
        PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
        packetbuf_copyfrom(&msg, sizeof(announce_msg_t));
        broadcast_send(&broadcast);
    }
    PROCESS_END();
}


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

