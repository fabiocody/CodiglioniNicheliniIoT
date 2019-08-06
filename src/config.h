// config.h

#ifndef CONFIG_H
#define CONFIG_H

#define UNICAST_CHANNEL 144
#define BROADCAST_CHANNEL 129
#define TRUCK_ADDR 8
#define BIN_TO_BIN_ALPHA 0.1
#define BIN_TO_TRUCK_ALPHA 1
#define MAX_RETRANSMISSIONS 10
#define FULL_THRESHOLD 40
#define ALERT_THRESHOLD (FULL_THRESHOLD-15)
#define ND_MIN_INTERVAL 30
#define ND_MAX_INTERVAL 60
#define MAX_NODES 256
#define BUSY_TIMER_DIVIDER 100


#endif // CONFIG_H
