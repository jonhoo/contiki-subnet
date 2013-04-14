/**
 * \file
 *         Periodically publishes sensor readings to meet all registered
 *         subscriptions
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#include "contiki.h"
#include "lib/publisher.h"
#include "lib/random.h"
#include <stdio.h>
#include <stdlib.h>
/*---------------------------------------------------------------------------*/
struct location node_location;
/*---------------------------------------------------------------------------*/
PROCESS(node_process, "Node");
AUTOSTART_PROCESSES(&node_process);
static humidity *get_humidity(struct location *l) {
  static humidity h;
  h.location.x = l->x;
  h.location.y = l->y;
  h.value = random_rand();
  printf("humidity @ <%03d, %03d> = %d\n", h.location.x, h.location.y, (int)h.value);
  return &h;
}
static pressure *get_pressure(struct location *l) {
  static pressure p;
  p.location.x = l->x;
  p.location.y = l->y;
  p.value = random_rand();
  printf("pressure @ <%03d, %03d> = %d\n", p.location.x, p.location.y, (int)p.value);
  return &p;
}
bool soft_filter_proxy(struct sfilter *f, enum reading_type t, void *data);
bool hard_filter_proxy(struct hfilter *f);
void aggregator_proxy(struct aggregator *a, struct esubscription *s, uint8_t items, void *datas[]);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct location l;

  PROCESS_BEGIN();
  l.x = random_rand() % 100;
  l.y = random_rand() % 100;
  printf("initialized location to <%03d, %03d>\n", l.x, l.y);

  // Initialize publisher
  publisher_start(&soft_filter_proxy, NULL, NULL, 10*CLOCK_SECOND);

  // Dynamic properties
  publisher_has(READING_HUMIDITY, sizeof(humidity));
  publisher_has(READING_PRESSURE, sizeof(pressure));

  while(1) {
    // When data is needed, read and publish
    PROCESS_WAIT_EVENT_UNTIL(publisher_in_need());

    // Only read sensor if needed
    // Humidity reading
    if (publisher_needs(READING_HUMIDITY)) {
      publisher_publish(READING_HUMIDITY, get_humidity(&l));
    }

    // Pressure reading
    if (publisher_needs(READING_PRESSURE)) {
      publisher_publish(READING_PRESSURE, get_pressure(&l));
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
/* proxy callbacks */
bool soft_filter_proxy(struct sfilter *f, enum reading_type t, void *data) {
  return false;
}

bool hard_filter_proxy(struct hfilter *f) {
  struct location *v, *n;
  switch (f->filter) {
    case BE_CLOSE_TO:
      v = &f->arg.loc;
      n = &node_location;

      if (abs(n->x - v->x) > 10) return true;
      if (abs(n->y - v->y) > 10) return true;
      /* fall-through */
    default:
      return false;
  }
}

void aggregator_proxy(struct aggregator *a, struct esubscription *s, uint8_t items, void *datas[]) {
  return;
}
/*---------------------------------------------------------------------------*/
