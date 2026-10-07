#ifndef BARBERSHOP_EVENT_QUEUE_H
#define BARBERSHOP_EVENT_QUEUE_H
#include "types.h"
#include <stdbool.h>

#define MAX_ELEMENTS 10000

typedef struct {
  Event events[MAX_ELEMENTS];
  int size;
}EventQueue;

void event_queue_init(EventQueue* eq);
bool event_queue_push(EventQueue* eq, Event event);
bool event_queue_pop(EventQueue* eq , Event* event);
bool event_queue_is_empty(const EventQueue* eq);
#endif