/**
 * \file
 *         Subscribe to sensor readings
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#include "contiki.h"
#include "subscriber.h"
#include "readings.h"
#include <stdlib.h>
/*---------------------------------------------------------------------------*/
#define MAX(a,b) (a>b?a:b)
/*---------------------------------------------------------------------------*/
static rimeaddr_t nodes[20];
static int numnodes = 0;
/**
 * \brief
 *          Handler for when a new sensor reading is available. Simply stores
 *          the node address if it is new to us.
 * \param r
 *          The new reading
 */
static void on_reading(struct readings *r) {
  int i = 0;
  for (;i < MAX(numnodes, 20);i++) {
    if (nodes[i] == r->node) {
      return;
    }
  }

  nodes[numnodes%20] = r->node;
  numnodes++;
}
/*---------------------------------------------------------------------------*/
PROCESS(sink_process, "Sink");
AUTOSTART_PROCESSES(&sink_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sink_process, ev, data)
{
  static struct etimer et;
  static struct subscriber ss;

  PROCESS_BEGIN();

  // Initialize subscriber
  subscriber_start(&ss, &on_reading);

  // Static properties
  subscriber_subscribe(&ss, READING_LOCATION, INT_MAX, sizeof(struct location));
  subscriber_subscribe(&ss, READING_HUMIDITY,      60, sizeof(double));
  subscriber_subscribe(&ss, READING_PRESSURE,     120, sizeof(double));

  while(1) {
    etimer_set(&et, CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // Update display of nodes
    for (int i = 0; i < MAX(numnodes, 20); i++) {
      struct location *l = subscriber_data(&ss, nodes[i], READING_LOCATION);
      double *humidity   = subscriber_data(&ss, nodes[i], READING_HUMIDITY);
      double *pressure   = subscriber_data(&ss, nodes[i], READING_PRESSURE);

      if (humidity != NULL) {
        printf("Node at <%03d, %03d> has humidity %.2f\n", l->x, l->y, *humidity);
      }
      if (pressure != NULL) {
        printf("Node at <%03d, %03d> has pressure %.2f\n", l->x, l->y, *pressure);
      }
    }

    etimer_reset(&et);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
