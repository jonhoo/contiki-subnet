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

/*---------------------------------------------------------------------------*/
/* private functions */
static void on_ondata(int sink, subid_t subid, void *data);
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
  int i;
  on_reading = cb;
  pubsub_init(&callbacks);

  for (i = 0; i < PUBSUB_MAX_SUBSCRIPTIONS; i++) {
    is[i] = i;
  }
}

subid_t subscriber_subscribe(struct subscription *s) {
  subid_t subid = pubsub_subscribe(s);
  ctimer_set(&resubscribe[subid], PUBSUB_RESEND_INTERVAL, &on_resubscribe, &is[subid]);
  return subid;
}
subid_t subscriber_replace(subid_t subid, struct subscription *s) {
  subscriber_unsubscribe(subid);
  return subscriber_subscribe(s);
}
void subscriber_unsubscribe(subid_t subid) {
  ctimer_stop(&resubscribe[subid]);
  pubsub_unsubscribe(subid);
}

const struct subscription *subscriber_subscription(subid_t subid) {
  int mysinkid = pubsub_myid();
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
  int subid = *((int *)subidp);
  pubsub_resubscribe(subid);
  ctimer_restart(&resubscribe[subid]);
}
static void on_ondata(int sink, subid_t subid, void *data) {
  if (sink == pubsub_myid() && on_reading != NULL) {
    on_reading(subid, data);
  }
}
/*---------------------------------------------------------------------------*/
/** @} */
