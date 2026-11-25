#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define MAX_TEXT_LEN 256
#define MAX_CLIENTS  128
#define MAX_CHAIRS   64

typedef enum {
  MSG_HELLO_BARBER = 1,
  MSG_HELLO_CUSTOMER = 2,
  MSG_HELLO_OBSERVER = 3,
  MSG_ASSIGN_CUSTOMER = 10,
  MSG_HAIRCUT_DONE = 11,
  MSG_CUSTOMER_WAIT = 20,
  MSG_CUSTOMER_SERVED = 21,
  MSG_CUSTOMER_LEFT = 22,
  MSG_OBSERVER_EVENT = 30
} MsgType;

typedef struct {
  int32_t type;
  int32_t id;
  char text[MAX_TEXT_LEN];
} Packet;

#endif