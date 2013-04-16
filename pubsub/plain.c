/**
 * \file
 *         Subscribe to sensor readings
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#include "contiki.h"
#include "lib/subscriber.h"
#include "pubsub-config.h"
#include <stdio.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
#define MAX(a,b) (a>b?a:b)
/*---------------------------------------------------------------------------*/
static void on_reading(subid_t subid, void *data) {
  const struct subscription *s = subscriber_subscription(subid);
  struct locshort r;
  memcpy(&r, data, sizeof(struct locshort));
  switch (s->sensor) {
    case READING_HUMIDITY:
    {
      printf("got humidity @ <%03d, %03d> = %d\n", r.location.x, r.location.y, r.value);
      break;
    }
    case READING_PRESSURE:
    {
      printf("got pressure @ <%03d, %03d> = %d\n", r.location.x, r.location.y, r.value);
      break;
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS(sink_process, "Sink");
AUTOSTART_PROCESSES(&sink_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(sink_process, ev, data)
{
  struct subscription s;

  PROCESS_BEGIN();

  /* initialize subscriber */
  subscriber_start(&on_reading);

  /* no special stuff here */
  s.soft.filter = NO_SOFT_FILTER;
  s.hard.filter = NO_HARD_FILTER;
  s.aggregator.aggregator = NO_AGGREGATION;

  /* subscribe to humidity */
  s.interval = 15*CLOCK_SECOND;
  s.sensor = READING_HUMIDITY;
  subscriber_subscribe(&s);
  printf("subscribed to humidity\n");

  /* subscribe to pressure */
  s.interval = 30*CLOCK_SECOND;
  s.sensor = READING_PRESSURE;
  subscriber_subscribe(&s);
  printf("subscribed to pressure\n");

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
