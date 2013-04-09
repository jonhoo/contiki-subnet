/**
 * \addtogroup pubsub
 * @{
 */

/**
 * \file   Publisher node implementation
 * \author Jon Gjengset <jon@tsp.io>
 */
#include "lib/pubsub/publisher.h"
#include "sys/ctimer.h"

/*---------------------------------------------------------------------------*/
/* private functions */
static void on_errpub();
static void on_ondata(int sink, subid_t subid, void *data);
static void on_onsent(int sink, subid_t subid);
static void on_subscription(struct full_subscription *s);
static void on_unsubscription(struct full_subscription *old);
static void on_collect_timer_expired(void *tp);
static void on_aggregate_timer_expired(void *sinkp);
static void set_needs(enum reading_type t, bool need);
static void aggregate_trigger(int sink, bool added_data);
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
static int is[SUBNET_MAX_SINKS]; /* sometimes, I dislike C */

static struct ctimer collect[PUBSUB_MAX_SENSORS];
static size_t rsize[PUBSUB_MAX_SENSORS];

static bool needs[PUBSUB_MAX_SENSORS];
static int numneeds;

static bool (* soft_filter)(struct sfilter *f, enum reading_type t, void *data);
static bool (* hard_filter)(struct hfilter *f, enum reading_type t, void *data);
/*---------------------------------------------------------------------------*/
/* public function definitions */
void publisher_start(
  bool (* soft_filter_proxy)(struct sfilter *f, enum reading_type t, void *data),
  bool (* hard_filter_proxy)(struct hfilter *f, enum reading_type t, void *data)
) {
  clock_time_t max = ~0;
  int i;

  soft_filter = soft_filter_proxy;
  hard_filter = hard_filter_proxy;

  etarget = PROCESS_CURRENT();
  pubsub_init(&callbacks);
  numneeds = 0;
  for (i = 0; i < PUBSUB_MAX_SENSORS; i++) {
    rsize[i] = 0;
    needs[i] = false;

    /* make sure we will chos eany period over this one */
    collect[i].etimer.timer.interval = max;
  }

  for (i = 0; i < SUBNET_MAX_SINKS; i++) {
    /* because ctimer_set needs a void* to pass to the callback, we can't just
     * pass &i since it will not point the same place when the callback is
     * invoked. Instead, we do this ugly workaround. We set up a static integer
     * array where each index points to its own value. We can then use the
     * address of each element for the pointer to pass to the callback. Yay :( */
    is[i] = i;
    ctimer_set(&aggregate[i], max, &on_aggregate_timer_expired, &is[i]);
    ctimer_stop(&aggregate[i]);

    /* makes ctimer_expired return true before the timer was started */
    aggregate[i].etimer.p = PROCESS_NONE;
  }
}
void publisher_has(enum reading_type t, size_t sz) {
  rsize[t] = sz;
}
bool publisher_in_need() {
  return numneeds > 0;
}
bool publisher_needs(enum reading_type t) {
  return needs[t];
}
void publisher_publish(enum reading_type t, void *reading) {
  struct full_subscription *s = NULL;
  bool added_data;
  set_needs(t, false);

  while (pubsub_next_subscription(&s)) {
    if (s->in.sensor == t) {
      /* don't add data if it doesn't pass the hard filter */
      if (hard_filter(&s->in.hard, t, reading)) {
        continue;
      }

      if (soft_filter(&s->in.soft, t, reading)) {
        /* soft filtering means we don't send a value, but we still need to
         * publish the subscription so that other nodes may hear it */
        added_data = pubsub_add_data(s->sink, s->subid, reading, 0);
      } else {
        added_data = pubsub_add_data(s->sink, s->subid, reading, rsize[t]);
      }

      aggregate_trigger(s->sink, added_data);
    }
  }
}
/*---------------------------------------------------------------------------*/
/* private function definitions */
static void on_subscription(struct full_subscription *s) {
  struct ctimer c = collect[s->in.sensor];
  if (s->in.interval < c.etimer.timer.interval) {
    ctimer_set(&c, s->in.interval, &on_collect_timer_expired, &s->in.sensor);
  }
}
static void on_unsubscription(struct full_subscription *old) {
  struct full_subscription *s = NULL;
  struct ctimer c = collect[s->in.sensor];
  ctimer_stop(&c);
  clock_time_t max = ~0;
  clock_time_t min = max;

  enum reading_type t = old->in.sensor;

  while (pubsub_next_subscription(&s)) {
    if (s->in.sensor != t) continue;
    if (s == old) continue;

    if (s->in.interval < min) {
      min = s->in.interval;
    }
  }

  if (min == max) {
    /* we now have no subscriptions for this timer, so no need to start it */
    /* we have to set the interval to max for the check in on_subscription to
     * keep working */
    c.etimer.timer.interval = max;
    return;
  }

  ctimer_set(&c, min, &on_collect_timer_expired, &s->in.sensor);
}
static void aggregate_trigger(int sink, bool added_data) {
  /* if last add failed, we should send the packet straightaway */
  /* TODO: send before full? */
  if (!added_data) {
    on_aggregate_timer_expired(&sink);
  }

  if (ctimer_expired(&aggregate[sink])) {
    ctimer_restart(&aggregate[sink]);
  }
}
static void on_ondata(int sink, subid_t subid, void *data) {
  struct full_subscription *s = find_subscription(sink, subid);
  bool added_data = pubsub_add_data(sink, subid, data, rsize[s->in.sensor]);
  aggregate_trigger(sink, added_data);
}
static void on_errpub() {
  /* TODO */
}
static void on_onsent(int sink, subid_t subid) {
  /* TODO */
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
  if (rsize[t] == 0) {
    /* node doesn't have this sensor */
    publisher_publish(t, NULL);
  } else {
    set_needs(t, true);
    process_post(etarget, PROCESS_EVENT_PUBLISH, tp);
  }

  ctimer_reset(&collect[t]);
}
static void on_aggregate_timer_expired(void *sinkp) {
  int sink = *((int *)sinkp);
  /* TODO: call aggregator */
  pubsub_publish(sink);
}
/*---------------------------------------------------------------------------*/
/** @} */
