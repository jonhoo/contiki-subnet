/**
 * \file
 *         Subscribe to sensor readings more often and within area
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#include "contiki.h"
#include "lib/subscriber.h"
#include "callbacks.c"
#include <stdio.h>
#include <string.h>
/*---------------------------------------------------------------------------*/
#define MAX(a,b) (a>b?a:b)
#define MAX_DISTANCE 20.0
#define MIN_HUMIDITY 0.5
#define MIN_PRESSURE 0.7
#define MAX_PRESSURE 0.9
/*---------------------------------------------------------------------------*/
static struct locdouble readings[5][2];
static int numreadings = 0;

static void on_reading(subid_t subid, void *data) {
  memcpy(&readings[numreadings%5][subid], data, sizeof(struct locdouble));
  numreadings++;
}
/*---------------------------------------------------------------------------*/
PROCESS(van_process, "Van");
AUTOSTART_PROCESSES(&van_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(van_process, ev, data)
{
  static struct etimer et;
  static struct location myloc = { 20, 10 };
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
  s.hard.filter = BE_CLOSE_TO;
  s.hard.arg.loc = myloc;
  s.aggregator.a = NO_AGGREGATION;

  /* subscribe to humidity */
  s.interval = 10;
  s.sensor = READING_HUMIDITY;
  subscriber_subscribe(&s);

  /* subscribe to pressure */
  s.interval = 20;
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
