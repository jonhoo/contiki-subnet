/**
 * \addtogroup pubsub
 * @{
 */

/**
 * \file   Subscriber node implementation
 * \author Jon Gjengset <jon@tsp.io>
 */
#include "lib/subscriber.h"
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
static void on_ondata(short sink, subid_t subid, void *data);
/*---------------------------------------------------------------------------*/
/* private members */
static struct pubsub_callbacks callbacks = {
  NULL,
  on_ondata,
  NULL,
  NULL,
  NULL
};
static void (*on_reading)(subid_t subid, void *data);
static struct ctimer resubscribe[PUBSUB_MAX_SUBSCRIPTIONS];
static subid_t is[PUBSUB_MAX_SUBSCRIPTIONS];
static void on_resubscribe(void *subidp);
/*---------------------------------------------------------------------------*/
/* public function definitions */
void subscriber_start(void (*cb)(subid_t subid, void *data)) {
  subid_t i;
  on_reading = cb;
  pubsub_init(&callbacks);

  for (i = 0; i < PUBSUB_MAX_SUBSCRIPTIONS; i++) {
    is[i] = i;
  }
}

subid_t subscriber_subscribe(struct subscription *s) {
  PRINTF("subscriber: adding new subscription\n");
  subid_t subid = pubsub_subscribe(s);
  PRINTF("subscriber: got id %d, starting timer with interval %lu\n", subid, PUBSUB_RESEND_INTERVAL);
  ctimer_set(&resubscribe[subid], PUBSUB_RESEND_INTERVAL, &on_resubscribe, &is[subid]);
  return subid;
}
subid_t subscriber_replace(subid_t subid, struct subscription *s) {
  subscriber_unsubscribe(subid);
  return subscriber_subscribe(s);
}
void subscriber_unsubscribe(subid_t subid) {
  PRINTF("subscriber: removing subscription %d, stopping timer\n", subid);
  ctimer_stop(&resubscribe[subid]);
  pubsub_unsubscribe(subid);
}

const struct subscription *subscriber_subscription(subid_t subid) {
  short mysinkid = pubsub_myid();
  if (mysinkid == -1) {
    return NULL;
  }

  struct full_subscription *s = find_subscription(mysinkid, subid);
  if (s == NULL) {
    return NULL;
  }

  return &s->in;
}

void subscriber_close() {
  pubsub_close();
}
/*---------------------------------------------------------------------------*/
/* private function definitions */
static void on_resubscribe(void *subidp) {
  subid_t subid = *((subid_t *)subidp);
  PRINTF("subscriber: resubscribing to %d\n", subid);
  pubsub_resubscribe(subid);
  ctimer_restart(&resubscribe[subid]);
  PRINTF("subscriber: timer reset\n");
}
static void on_ondata(short sink, subid_t subid, void *data) {
  PRINTF("subscriber: got data for %d:%d\n", sink, subid);
  if (sink == pubsub_myid() && on_reading != NULL) {
    PRINTF("subscriber: oh, it's for us!\n");
    on_reading(subid, data);
  }
}
/*---------------------------------------------------------------------------*/
/** @} */
