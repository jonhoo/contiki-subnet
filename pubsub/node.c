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
  return &h;
}
static pressure *get_pressure(struct location *l) {
  static pressure p;
  p.location.x = l->x;
  p.location.y = l->y;
  p.value = r(100);
  return &p;
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct location l;

  PROCESS_BEGIN();
  l.x = random_rand() % 100;
  l.y = random_rand() % 100;

  // Initialize publisher
  publisher_start(&soft_filter_proxy, NULL, NULL);

  // Dynamic properties
#if HAS_HUMIDITY
  publisher_has(READING_HUMIDITY, sizeof(humidity));
#endif
#if HAS_PRESSURE
  publisher_has(READING_PRESSURE, sizeof(pressure));
#endif

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
