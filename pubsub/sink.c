/**
 * \file
 *         Subscribe to sensor readings
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#include "contiki.h"
#include "lib/subscriber.h"
#include "pubsub-config.h"
#include "callbacks.c"
#include <stdio.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
#define MAX(a,b) (a>b?a:b)
/*---------------------------------------------------------------------------*/
static struct locdouble readings[5][2];
static int numreadings = 0;

static void on_reading(subid_t subid, void *data) {
  memcpy(&readings[numreadings%5][subid], data, sizeof(struct locdouble));
  printf("got reading for subscription %d\n", subid);
  numreadings++;
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

  /* no special stuff here */
  s.soft.filter = NO_SOFT_FILTER;
  s.hard.filter = NO_HARD_FILTER;
  s.aggregator.a = NO_AGGREGATION;

  /* subscribe to humidity */
  s.interval = 20;
  s.sensor = READING_HUMIDITY;
  subscriber_subscribe(&s);
  printf("subscribed to humidity\n");

  /* subscribe to pressure */
  s.interval = 40;
  s.sensor = READING_PRESSURE;
  subscriber_subscribe(&s);
  printf("subscribed to pressure\n");

  while(1) {
    etimer_set(&et, CLOCK_SECOND);
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));

    // Update display of nodes
    for (i = 0; i < 5; i++) {
      humidity h = readings[i][0];
      pressure p = readings[i][1];

      if (h.value != -1) {
        printf("Node at <%03d, %03d> has humidity ~%d\n", h.location.x, h.location.y, (int)h.value);
      }
      if (p.value != -1) {
        printf("Node at <%03d, %03d> has pressure ~%d\n", p.location.x, p.location.y, (int)p.value);
      }
    }

    etimer_reset(&et);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
