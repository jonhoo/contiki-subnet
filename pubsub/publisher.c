/**
 * \addtogroup pubsub
 * @{
 */

/**
 * \file   Publisher node implementation
 * \author Jon Gjengset <jon@tsp.io>
 */
#include "lib/publisher.h"
#include "sys/ctimer.h"

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
/* private functions */
static void on_errpub();
static void on_ondata(short sink, subid_t subid, void *data);
static void on_onsent(short sink, subid_t subid);
static void on_subscription(struct esubscription *s);
static void on_unsubscription(struct esubscription *old);
static void on_collect_timer_expired(void *tp);
static void on_aggregate_timer_expired(void *sinkp);
static void set_needs(enum reading_type t, bool need);
static void aggregate_trigger(short sink, bool added_data);
/*---------------------------------------------------------------------------*/
/* private members */
static struct process *etarget;
static struct pubsub_callbacks callbacks = {
  on_errpub,
  on_ondata,
  on_onsent,

  on_subscription,
  on_unsubscription
};

static struct ctimer aggregate[SUBNET_MAX_SINKS];
static short is[SUBNET_MAX_SINKS]; /* sometimes, I dislike C */

static struct ctimer collect[PUBSUB_MAX_SENSORS];
static dlen_t rsize[PUBSUB_MAX_SENSORS];

static bool needs[PUBSUB_MAX_SENSORS];
static uint8_t numneeds;

static bool (* soft_filter)(struct sfilter *f, enum reading_type t, void *data);
static bool (* hard_filter)(struct hfilter *f);
static void (* aggregator)(struct aggregator *a, short sink, subid_t subid, uint8_t items, void *datas[]);
/*---------------------------------------------------------------------------*/
/* public function definitions */
void publisher_start(
  bool (* soft_filter_proxy)(struct sfilter *f, enum reading_type t, void *data),
  bool (* hard_filter_proxy)(struct hfilter *f),
  void (* aggregator_proxy)(struct aggregator *a, short sink, subid_t subid, uint8_t items, void *datas[]),
  clock_time_t agg_interval
) {
  clock_time_t max = (~((clock_time_t)0) / 2);
  uint8_t i;

  soft_filter = soft_filter_proxy;
  hard_filter = hard_filter_proxy;
  aggregator = aggregator_proxy;

  etarget = PROCESS_CURRENT();
  pubsub_init(&callbacks);
  numneeds = 0;
  for (i = 0; i < PUBSUB_MAX_SENSORS; i++) {
    rsize[i] = 0;
    needs[i] = false;

    /* make sure we will chose any period over this one */
    collect[i].etimer.timer.interval = max;
  }

  for (i = 0; i < SUBNET_MAX_SINKS; i++) {
    /* because ctimer_set needs a void* to pass to the callback, we can't just
     * pass &i since it will not point the same place when the callback is
     * invoked. Instead, we do this ugly workaround. We set up a static integer
     * array where each index points to its own value. We can then use the
     * address of each element for the pointer to pass to the callback. Yay :( */
    is[i] = i;
    ctimer_set(&aggregate[i], agg_interval, &on_aggregate_timer_expired, &is[i]);
    ctimer_stop(&aggregate[i]);

    /* makes ctimer_expired return true before the timer was started */
    aggregate[i].etimer.p = PROCESS_NONE;
  }
}
void publisher_has(enum reading_type t, dlen_t sz) {
  rsize[t] = sz;
}
bool publisher_in_need() {
  return numneeds > 0;
}
bool publisher_needs(enum reading_type t) {
  return needs[t];
}
void publisher_publish(enum reading_type t, void *reading) {
  struct wsubscription s;
  bool added_data = true;
  set_needs(t, false);
  PRINTF("publisher: incoming reading for sensor %d\n", t);
  PRINTF("publisher: resetting collection timer with interval %lu\n", collect[t].etimer.timer.interval);
  ctimer_restart(&collect[t]);

  s.sink = -1;
  while (pubsub_next_subscription(&s)) {
    if (s.esub->in.sensor == t) {
      /* don't add data if it doesn't pass the hard filter */
      if (hard_filter != NULL && hard_filter(&s.esub->in.hard)) continue;

      PRINTF("publisher: applying to subscription %d:%d\n", s.sink, s.subid);

      if (!soft_filter(&s.esub->in.soft, t, reading)) {
        added_data = pubsub_add_data(s.sink, s.subid, reading, rsize[t]);
      } else {
        PRINTF("publisher: reading soft filtered, so not writing\n");
      }

      aggregate_trigger(s.sink, added_data);
    }
  }
}
/*---------------------------------------------------------------------------*/
/* private function definitions */
static void on_subscription(struct esubscription *s) {
  struct ctimer *c = &collect[s->in.sensor];
  PRINTF("publisher: got new subscription for sensor: %d\n", s->in.sensor);
  if (hard_filter != NULL && hard_filter(&s->in.hard)) {
    PRINTF("publisher: subscription ignored - hard filtered\n");
    return;
  }

  if (s->in.interval < c->etimer.timer.interval) {
    PRINTF("publisher: new interval %lu is lower than current %lu, setting ctimer\n", s->in.interval, c->etimer.timer.interval);
    ctimer_set(c, s->in.interval, &on_collect_timer_expired, &s->in.sensor);
    on_collect_timer_expired(&s->in.sensor);
  } else {
    PRINTF("publisher: current interval %lu < subscription's %lu, ignoring\n", c->etimer.timer.interval, s->in.interval);
  }
}
static void on_unsubscription(struct esubscription *old) {
  struct wsubscription s;
  enum reading_type t = old->in.sensor;
  struct ctimer c = collect[t];
  ctimer_stop(&c);
  clock_time_t max = (~((clock_time_t)0) / 2);
  clock_time_t min = max;

  s.sink = -1;
  while (pubsub_next_subscription(&s)) {
    if (s.esub->in.sensor != t) continue;
    if (s.esub == old) continue;

    if (s.esub->in.interval < min) {
      min = s.esub->in.interval;
    }
  }

  if (min == max) {
    /* we now have no subscriptions for this timer, so no need to start it */
    /* we have to set the interval to max for the check in on_subscription to
     * keep working */
    c.etimer.timer.interval = max;
    PRINTF("publisher: no other subscriptions for this sensor, stopping timer\n");
    return;
  }

  PRINTF("publisher: new sample interval is %lu\n", min);
  ctimer_set(&c, min, &on_collect_timer_expired, &s.esub->in.sensor);
}
static void aggregate_trigger(short sink, bool added_data) {
  /* if last add failed, we should send the packet straightaway */
  /* TODO: send before full? */
  if (!added_data) {
    PRINTF("publisher: packet probably full - attempting to send\n");
    on_aggregate_timer_expired(&sink);
  }

  if (ctimer_expired(&aggregate[sink])) {
    PRINTF("publisher: aggregation timer expired, restarting\n");
    ctimer_restart(&aggregate[sink]);
  }
}
static void on_ondata(short sink, subid_t subid, void *data) {
  PRINTF("publisher: heard data from upstream - adding\n");

  struct esubscription *s = find_subscription(sink, subid);
  bool added_data = pubsub_add_data(sink, subid, data, rsize[s->in.sensor]);
  aggregate_trigger(sink, added_data);
}
static void on_errpub() {
  PRINTF("publisher: data publishing failed - could not forward packet\n");
}
static void on_onsent(short sink, subid_t subid) {
  PRINTF("publisher: data published successfully\n");
}
static void set_needs(enum reading_type t, bool need) {
  bool n = needs[t];
  if (n && !need) {
    numneeds--;
  } else if (!n && need) {
    numneeds++;
  }
  needs[t] = need;
}
static void on_collect_timer_expired(void *tp) {
  enum reading_type t = *((enum reading_type *)tp);
  if (rsize[t] != 0) {
    PRINTF("publisher: collecting time for sensor %d\n", t);
    set_needs(t, true);
    process_post(etarget, PROCESS_EVENT_PUBLISH, tp);
  } else {
    PRINTF("publisher: time for sensor %d, but not present, so skipping\n", t);
  }
}
static void on_aggregate_timer_expired(void *sinkp) {
  static void *payloads[PUBSUB_MAX_SUBSCRIPTIONS];
  struct esubscription *sub = NULL;
  short sink = *((short *)sinkp);
  subid_t maxsub = last_subscription(sink);
  short num, i, subid;

  PRINTF("publisher: time to send out a data packet to sink %d\n", sink);

  pubsub_writeout(sink);
  for (subid = 0; subid <= maxsub; subid++) {
    sub = find_subscription(sink, subid);

    if (is_active(sub)) {
      PRINTF("publisher: subscription %d:%d is active", sink, subid);
      if (hard_filter != NULL && hard_filter(&sub->in.hard)) {
        PRINTF(", but hard filtered\n");
        continue;
      }
      PRINTF("\n");

      num = extract_data(sink, subid, payloads);
      if (num == 0) {
        PRINTF("publisher: no data for subscription %d, adding\n", subid);
        pubsub_add_data(sink, subid, NULL, 0);
      } else if (aggregator == NULL) {
        PRINTF("publisher: no aggregator for subscription %d, adding all %d\n", subid, num);
        for (i = 0; i < num; i++) {
          /* it is safe to use rsize[t] here because we know received values
           * won't have been aggregated either */
          pubsub_add_data(sink, subid, payloads[i], rsize[sub->in.sensor]);
        }
      } else {
        PRINTF("publisher: calling aggregator for %d values in subscription %d\n", num, subid);
        aggregator(&sub->in.aggregator, sink, subid, num, payloads);
        /* aggregator will call pubsub_add_data(sink, subid, ...) */
      }
    }
  }
  pubsub_writein();
  pubsub_publish(sink);
}
/*---------------------------------------------------------------------------*/
/** @} */
