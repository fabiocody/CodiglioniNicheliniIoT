// messages.h


// IDEAS
// msg_id


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


#define ALERT_MSG   1
#define TRUCK_MSG   2
#define MOVE_MSG    3
#define MOVE_REPLY  4
#define TRASH_MSG   5
