/**
 * \file
 *         Subscribe to sensor readings more often and within area
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#include "contiki.h"
#include "lib/subscriber.h"
#include "subnet-config.h"
#include "dev/serial-line.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
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
      printf("got: humidity @ <%03d, %03d> = %d\n", r.location.x, r.location.y, r.value);
      break;
    }
    case READING_PRESSURE:
    {
      printf("got: pressure @ <%03d, %03d> = %d\n", r.location.x, r.location.y, r.value);
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

  /* initialize subscriber */
  subscriber_start(&on_reading);

  /* no special stuff here */
  s.soft.filter = NO_SOFT_FILTER;
  s.hard.filter = BE_CLOSE_TO;
  s.hard.arg.loc = l;
  s.aggregator.aggregator = NO_AGGREGATION;

  /* subscribe to humidity */
  s.interval = 5*CLOCK_SECOND;
  s.sensor = READING_HUMIDITY;
  subscriber_subscribe(&s);
  printf("subscribed to humidity\n");

  /* subscribe to pressure */
  s.interval = 10*CLOCK_SECOND;
  s.sensor = READING_PRESSURE;
  subscriber_subscribe(&s);
  printf("subscribed to pressure\n");

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
