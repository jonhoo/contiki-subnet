/**
 * \file
 *         Subscribe to sensor readings
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#include "contiki.h"
#include "lib/subscriber.h"
#include "callbacks.c"
#include <stdlib.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
#define MAX(a,b) (a>b?a:b)
/*---------------------------------------------------------------------------*/
static struct locdouble readings[5][2];

static void on_reading(int sink, subid_t subid, void *data) {
  memcpy(&readings[sink][subid], data, sizeof(struct locdouble));
}
/*---------------------------------------------------------------------------*/
PROCESS(sink_process, "Sink");
AUTOSTART_PROCESSES(&sink_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sink_process, ev, data)
{
  static struct etimer et;
  struct subscription s;
  int i,j;

  PROCESS_BEGIN();

  for (i = 0; i < 5; i++) {
    for (j = 0; j < 5; j++) {
      readings[i][j].value = -1;
    }
  }

  /* initialize subscriber */
  subscriber_start(&on_reading);

  /* subscribe to humidity */
  s.interval = 20;
  s.sensor = READING_HUMIDITY;
  subscriber_subscribe(&s);

  /* subscribe to pressure */
  s.interval = 40;
  s.sensor = READING_PRESSURE;
  subscriber_subscribe(&s);

  while(1) {
    etimer_set(&et, CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // Update display of nodes
    for (i = 0; i < 5; i++) {
      humidity h = readings[i][0];
      pressure p = readings[i][1];

      if (h.value != -1) {
        printf("Node at <%03d, %03d> has humidity %.2f\n", h.location.x, h.location.y, h.value);
      }
      if (p.value != -1) {
        printf("Node at <%03d, %03d> has pressure %.2f\n", p.location.x, p.location.y, p.value);
      }
    }

    etimer_reset(&et);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
