#include "barbershop.h"
#include "logger.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static double rand_range(double min, double max) {
  return min + ((double)rand() / (double)RAND_MAX) * (max - min);
}
bool shop_init(BarberShop* shop, Config config) {
  shop->config = config;
  shop->cur_time = 0.0;
  shop->waiting_count = 0;
  shop->next_customer_id = 1;
  shop->statistics.total_arrived = 0;
  shop->statistics.total_served = 0;
  shop->statistics.total_out = 0;
  shop->statistics.total_waitind_time = 0.0;
  shop->barbers= (Barber*)malloc(sizeof(Barber)*config.barber_count);
  if (!shop->barbers) {
    return false;
  }
  for (int i = 0 ; i < config.barber_count ; i++) {
    shop->barbers[i].id = i+1;
    shop->barbers[i].state = BARBER_SLEEP;
    shop->barbers[i].current_customer_id = -1;
    shop->barbers[i].total_customers =0;
  }
  shop->waiting_chairs = (Customer*)malloc(sizeof(Customer)*config.waitind_chairs);
  if (!shop->waiting_chairs) {
    free(shop->barbers);
    return false;
  }
  event_queue_init(&shop->eq);
  return true;
}
void shop_destroy(BarberShop* shop) {
  if (shop->barbers) {
    free(shop->barbers);
  }
  if (shop->waiting_chairs) {
    free(shop->waiting_chairs);
  }
}
void shop_plan_initial_arrival(BarberShop* shop) {
  double t = 0.0;
  for (int i = 0 ; i < shop->config.total_customers ; i++) {
    t += rand_range(shop->config.min_arrival_interval, shop->config.max_arrival_interval);
    if (t > shop->config.max_sim_time) {
      break;
    }
    Event event;
    event.time = t;
    event.type = CUSTOMER_ARRIVE;
    event.customer_id = shop->next_customer_id++;
    event.barber_id = -1;
    event_queue_push(&shop->eq, event);
  }
}

static int find_sleeping_barber(const BarberShop* shop) {
  for (int i = 0 ; i < shop->config.barber_count ; i++) {
    if (shop->barbers[i].state == BARBER_SLEEP) {
      return i;
    }
  }
  return -1;
}

void shop_check_invariants(const BarberShop* shop) {
  assert(shop->waiting_count <= shop->config.waitind_chairs);
  for (int i = 0 ; i < shop->config.barber_count ; i++) {
    if (shop->barbers[i].state == BARBER_SLEEP) {
      assert(shop->barbers[i].current_customer_id == -1);
    }
  }
}
void shop_handle_event(BarberShop* shop , Event event) {
  shop->cur_time = event.time;
  if (event.type == CUSTOMER_ARRIVE) {
    shop->statistics.total_arrived++;
    logger_log(shop->cur_time, "Посетитель #%d вошел в парикмахерскую.", event.customer_id);
    int b_id = find_sleeping_barber(shop);
    if (b_id != -1) {
      Barber *b = &shop->barbers[b_id];
      b->state = BARBER_WORK;
      b->current_customer_id = event.customer_id;
      logger_log(shop->cur_time , "Парикмахер #%d просыпается и берет посетителя #%d" , b->id , event.customer_id);
      logger_log(shop->cur_time , "Парикмахер #%d начинает стричь поситителя #%d" , b->id , event.customer_id);
      double duration = rand_range(shop->config.min_haircut_time, shop->config.max_haircut_time);
      Event finish = {
        .time = shop->cur_time + duration,
        .type = CUSTOMER_FINISHED,
        .customer_id = event.customer_id,
        .barber_id = b->id
      };
      event_queue_push(&shop->eq, finish);
    }
    else if (shop->waiting_count < shop->config.waitind_chairs) {
      Customer c = {
        .id = event.customer_id,
        .arrival_time = shop->cur_time,
        .start_time = 0.0,
        .finished_time = 0.0
      };
      shop->waiting_chairs[shop->waiting_count++] = c;
      logger_log(shop->cur_time, "Мастера занеты .  Посетитель #%d занял стул в очередь ( очередь  #%d из #%d)" , event.customer_id, shop->waiting_count, shop->config.waitind_chairs);
    }
    else {
      shop->statistics.total_out++;
      logger_log(shop->cur_time , "Мест нет , посетитель #%d уходит", event.customer_id);
    }
  }
  else if (event.type == CUSTOMER_FINISHED) {
    int b_id = event.barber_id - 1;
    Barber* b = &shop->barbers[b_id];
    logger_log(shop->cur_time , "Парикмахер #%d закончил стрижку посетителю #%d",  b->id, event.customer_id);
    logger_log(shop->cur_time , "Посетитель #%d освобождает стул и уходит", event.customer_id);
    b->total_customers++;
    shop->statistics.total_served++;
    if (shop->waiting_count > 0) {
      Customer next = shop->waiting_chairs[0];
      for (int i = 0 ; i < shop->waiting_count - 1; i++) {
        shop->waiting_chairs[i] = shop->waiting_chairs[i + 1];
      }
      shop->waiting_count--;
      shop->statistics.total_waitind_time += (shop->cur_time - next.arrival_time);
      b->current_customer_id = next.id;
      logger_log(shop->cur_time, "Парикмахер #%d приглашает следующего посетителя #%d" , b->id, next.id);
      logger_log(shop->cur_time, "Парикмахер #%d начинает стричь посетителя #%d", b->id, next.id);
      double duration = rand_range(shop->config.min_haircut_time, shop->config.max_haircut_time);
      Event finish = {
        .time = shop->cur_time + duration,
        .type = CUSTOMER_FINISHED,
        .customer_id = next.id,
        .barber_id = b->id
      };
      event_queue_push(&shop->eq, finish);
    }
    else {
      b->state = BARBER_SLEEP;
      b->current_customer_id = -1;
      logger_log(shop->cur_time , "Очередь пуста. Парикмахер #%d засыпает", b->id);
    }
  }
  shop_check_invariants(shop);
}

void shop_print_statistics(const BarberShop* shop) {
  logger_log(shop->cur_time , "Всего поситителей пришло: %d", shop->statistics.total_arrived);
  logger_log(shop->cur_time , "Обслужено: %d", shop->statistics.total_served);
  logger_log(shop->cur_time , "Не обслужено: %d", shop->statistics.total_out);
  if (shop->statistics.total_served > 0) {
    logger_log(shop->cur_time , "Сред время ожидания: %.2f сек",shop->statistics.total_waitind_time /  shop->statistics.total_served);
  }
  for (int i = 0 ; i < shop->config.barber_count ; i++) {
    logger_log(shop->cur_time , "Парикмахер #%d обслужил клиентов: %d" , shop->barbers[i].id, shop->barbers[i].total_customers);
  }
}