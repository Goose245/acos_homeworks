#include "event_queue.h"

void event_queue_init(EventQueue* eq) {
  eq->size = 0;
}
bool event_queue_push(EventQueue* eq, Event event) {
  if (eq->size >= MAX_ELEMENTS) {
    return false;
  }
  int i = eq->size - 1;
  while (i >= 0 && eq->events[i].time > event.time) {
    eq->events[i + 1] = eq->events[i];
    i--;
  }
  eq->events[i + 1] = event;
  eq->size++;
  return true;
}

bool event_queue_pop(EventQueue* eq , Event* event) {
  if (eq->size == 0) {
    return false;
  }
  *event = eq->events[0];
  for (int i = 0 ; i < eq->size - 1; i++) {
    eq->events[i] = eq->events[i + 1];
  }
  eq->size--;
  return true;
}
bool event_queue_is_empty(const EventQueue* eq) {
  return eq->size == 0;
}