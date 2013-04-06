/**
 * \file
 *         Periodically publishes sensor readings to meet all registered
 *         subscriptions
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#include "contiki.h"
#include "lib/pubsub/publisher.h"
#include "readings.h"
#include <stdlib.h>
/*---------------------------------------------------------------------------*/
/**
 * \brief
 *          Handler for when a new subscription is registered
 * \param s
 *          The new subscription
 */
static void on_subscription(struct subscription *s) {}
/*---------------------------------------------------------------------------*/
PROCESS(node_process, "Node");
AUTOSTART_PROCESSES(&node_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(node_process, ev, data)
{
  static struct publisher ps;
  static struct location myloc = { 50, 50 };

  PROCESS_BEGIN();

  // Initialize publisher
  publisher_start(&ps, &on_subscription);

  // Static properties
  publisher_always_has(&ps, READING_LOCATION, &myloc, sizeof(struct location));

  // Dynamic properties
#if HAS_HUMIDITY
  publisher_has(&ps, READING_HUMIDITY, sizeof(double));
#endif
#if HAS_PRESSURE
  publisher_has(&ps, READING_PRESSURE, sizeof(double));
#endif

  while(1) {
    // When data is needed, read and publish
    PROCESS_WAIT_EVENT_UNTIL(publisher_in_need(&ps));

    // Only read sensor if needed
#if HAS_HUMIDITY
    // Humidity reading
    if (publisher_needs(&ps, READING_HUMIDITY)) {
      publisher_publish(&ps, READING_HUMIDITY, get_humidity());
    }
#endif

#if HAS_PRESSURE
    // Pressure reading
    if (publisher_needs(&ps, READING_PRESSURE)) {
      publisher_publish(&ps, READING_PRESSURE, get_pressure());
    }
#endif
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
