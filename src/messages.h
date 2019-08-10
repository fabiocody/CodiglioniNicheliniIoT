// messages.h

#include "contiki.h"


#ifndef MESSAGES_H
#define MESSAGES_H


// BROADCAST: BIN->BIN
#define     MOVE_MSG  1

// UNICAST: BIN->BIN
#define   MOVE_REPLY  2
#define    TRASH_MSG  3

// UNICAST: BIN->TRUCK
#define    ALERT_MSG  4
//#define    TRUCK_ACK  5

// UNICAST: TRUCK->BIN
#define    TRUCK_MSG  6


#define GET_MSG_TYPE(msg) (*(unsigned char *)msg)


typedef struct {
    unsigned char type;
    unsigned short x;
    unsigned short y;
    unsigned char id;
} alert_msg_t;


typedef struct {
    unsigned char type;
} truck_msg_t;


typedef struct {
    unsigned char type;
} move_msg_t;


typedef struct {
    unsigned char type;
    unsigned short x;
    unsigned short y;
} move_reply_t;


typedef struct {
    unsigned char type;
    unsigned int trash;
} trash_msg_t;


/*typedef struct {
    unsigned char type;
} truck_ack_t;*/


#endif // MESSAGES_H
