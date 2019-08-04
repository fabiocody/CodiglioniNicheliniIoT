// messages.h

#include "contiki.h"


// IDEAS
// msg_id


#define ANNOUNCE_MSG  0
#define    ALERT_MSG  1
#define    TRUCK_MSG  2
#define     MOVE_MSG  3
#define    TRASH_MSG  4
#define   MOVE_REPLY  5


typedef struct {
    unsigned char type;
} announce_msg_t;


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


#define GET_MSG_TYPE(msg) (*(unsigned char *)msg)
