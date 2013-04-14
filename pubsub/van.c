/**
 * \file
 *         Subscribe to sensor readings more often and within area
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
#define MIN_DEVIATION 1000
/*---------------------------------------------------------------------------*/
static void on_reading(subid_t subid, void *data) {
  const struct subscription *s = subscriber_subscription(subid);
  struct locshort r;
  memcpy(&r, data, sizeof(struct locshort));
  switch (s->sensor) {
    case READING_HUMIDITY:
    {
      printf("got humidity reading %d @ <%03d, %03d>\n", r.value, r.location.x, r.location.y);
      break;
    }
    case READING_PRESSURE:
    {
      printf("got pressure reading %d @ <%03d, %03d>\n", r.value, r.location.x, r.location.y);
      break;
    }
  }
}
/*---------------------------------------------------------------------------*/
PROCESS(van_process, "Van");
AUTOSTART_PROCESSES(&van_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(van_process, ev, data)
{
  struct subscription s;
  static struct location myloc = { 20, 10 };

  PROCESS_BEGIN();

  /* initialize subscriber */
  subscriber_start(&on_reading);

  /* no special stuff here */
  s.soft.filter = DEVIATION;
  s.soft.arg.deviation = MIN_DEVIATION;
  s.hard.filter = BE_CLOSE_TO;
  s.hard.arg.loc = myloc;
  s.aggregator.aggregator = NO_AGGREGATION;

  /* subscribe to humidity */
  s.interval = 10*CLOCK_SECOND;
  s.sensor = READING_HUMIDITY;
  subscriber_subscribe(&s);
  printf("subscribed to humidity\n");

  /* subscribe to pressure */
  s.interval = 20*CLOCK_SECOND;
  s.sensor = READING_PRESSURE;
  subscriber_subscribe(&s);
  printf("subscribed to pressure\n");

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
