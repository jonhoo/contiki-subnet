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
#include "callbacks.c"
/*---------------------------------------------------------------------------*/
PROCESS(node_process, "Node");
AUTOSTART_PROCESSES(&node_process);
static double r(double max) {
  return max * ((double)random_rand())/65535;
}
static humidity *get_humidity(struct location *l) {
  static humidity h;
  h.location.x = l->x;
  h.location.y = l->y;
  h.value = r(100);
  printf("humidity @ <%03d, %03d> = %d\n", h.location.x, h.location.y, (int)h.value);
  return &h;
}
static pressure *get_pressure(struct location *l) {
  static pressure p;
  p.location.x = l->x;
  p.location.y = l->y;
  p.value = r(100);
  printf("pressure @ <%03d, %03d> = %d\n", p.location.x, p.location.y, (int)p.value);
  return &p;
}
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
