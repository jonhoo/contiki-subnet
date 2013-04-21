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
#include "dev/serial-line.h"
#include <stdio.h>
#include <stdlib.h>
/*---------------------------------------------------------------------------*/
#define AVG(a,b) ((a+b)/2)
#define AVG_WINDOW 10
struct location node_location;
/*---------------------------------------------------------------------------*/
PROCESS(node_process, "Node");
AUTOSTART_PROCESSES(&node_process);
static humidity *get_humidity(struct location *l) {
  static humidity h;
  h.location.x = l->x;
  h.location.y = l->y;
  h.value = random_rand() % 25;
  printf("sense: humidity @ <%03d, %03d> = %d\n", h.location.x, h.location.y, h.value);
  return &h;
}
static pressure *get_pressure(struct location *l) {
  static pressure p;
  p.location.x = l->x;
  p.location.y = l->y;
  p.value = random_rand() % 25;
  printf("sense: pressure @ <%03d, %03d> = %d\n", p.location.x, p.location.y, p.value);
  return &p;
}
bool soft_filter_proxy(struct sfilter *f, enum reading_type t, void *data);
bool hard_filter_proxy(struct hfilter *f);
void aggregator_proxy(struct aggregator *a, short sink, subid_t subid, uint8_t items, void *datas[]);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct location l;
  static bool gotx = false;

  PROCESS_BEGIN();
  printf("acquiring position...\n");
  while (1) {
    // Wait for the next event.
    PROCESS_YIELD_UNTIL(ev == serial_line_event_message);

    if (!gotx) {
      l.x = atoi((char*)data);
      gotx = true;
    } else {
      l.y = atoi((char*)data);
      break;
    }
  }
  printf("location read: <%03d, %03d>\n", l.x, l.y);

  // Initialize publisher
  publisher_start(&soft_filter_proxy, &hard_filter_proxy, &aggregator_proxy, 10*CLOCK_SECOND);

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
  static short prevs[PUBSUB_MAX_SENSORS][AVG_WINDOW];
  static uint8_t num[PUBSUB_MAX_SENSORS];
  static uint8_t old[PUBSUB_MAX_SENSORS];
  switch (f->filter) {
    case DEVIATION:
    {
      uint8_t i;
      struct locshort *l = (struct locshort *) data;
      short sum = 0;

      prevs[t][old[t]] = l->value;
      old[t] = (old[t] + 1) % AVG_WINDOW;
      if (num[t] < AVG_WINDOW) {
        num[t]++;
        return false;
      }

      for (i = 0; i < num[t]; i++) {
        sum += prevs[t][i];
      }

      if (abs(sum/num[t] - l->value) < f->arg.deviation) {
        return true;
      }
      /* fall-through */
    }
    default:
      return false;
  }
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

void aggregator_proxy(struct aggregator *agg, short sink, subid_t subid, uint8_t items, void *datas[]) {
  int i;

  switch (agg->aggregator) {
    case LOCATION_AVG:
    {
      struct locshort *a,*b;
      int j, changes;
      do {
        changes = 0;
        for (i = 0; i < items; i++) {
          a = (struct locshort *) datas[i];
          if (a == NULL) continue;
          for (j = i+1; j < items; j++) {
            b = (struct locshort *) datas[j];
            if (b == NULL) continue;
            if (abs(a->location.x - b->location.x) > agg->arg.maxdist) continue;
            if (abs(a->location.y - b->location.y) > agg->arg.maxdist) continue;

            a->value = AVG(a->value, b->value);
            a->location.x = AVG(a->location.x, b->location.x);
            a->location.y = AVG(a->location.y, b->location.y);
            datas[j] = NULL;
            changes++;
          }
        }
      } while (changes != 0);

      for (i = 0; i < items; i++) {
        if (datas[i] == NULL) continue;
        pubsub_add_data(sink, subid, datas[i], sizeof(struct locshort));
      }
      break;
    }
    default:
      for (i = 0; i < items; i++) {
        pubsub_add_data(sink, subid, datas[i], sizeof(struct locshort));
      }
  }
}
/*---------------------------------------------------------------------------*/
