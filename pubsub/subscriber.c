/**
 * \addtogroup pubsub
 * @{
 */

/**
 * \file   Subscriber node implementation
 * \author Jon Gjengset <jon@tsp.io>
 */
#include "lib/pubsub/subscriber.h"

/*---------------------------------------------------------------------------*/
/* private functions */
static void on_ondata(struct subnet_conn *c, int sink, short subid, void *data);
/*---------------------------------------------------------------------------*/
/* private members */
static struct pubsub_callbacks callbacks = {
  NULL,
  on_ondata,
  NULL,
  NULL,
  NULL
};
static void (*on_reading)(short subid, void *data);
static ctimer resubscribe[PUBSUB_MAX_SUBSCRIPTIONS];
static short is[PUBSUB_MAX_SUBSCRIPTIONS];
/*---------------------------------------------------------------------------*/
/* public function definitions */
void subscriber_start(void (*cb)(short subid, void *data)) {
  on_reading = cb;
  pubsub_init(&callbacks);

  for (i = 0; i < PUBSUB_MAX_SUBSCRIPTIONS; i++) {
    is[i] = i;
  }
}

short subscriber_subscribe(struct subscription *s) {
  short subid = pubsub_subscribe(s);

  ctimer_set(&resubscribe[subid], PUBSUB_RESEND_INTERVAL, &on_resubscribe, &is[subid]);
}
short subscriber_replace(short subid, struct subscription *s) {
  subscriber_unsubscribe(subid);
  return subscriber_subscribe(s);
}
void subscriber_unsubscribe(short subid) {
  ctimer_stop(&resubscribe[subid]);
  pubsub_unsubscribe(subid);
}

const struct subscription *subscriber_subscription(short subid) {
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
/*---------------------------------------------------------------------------*/
/* private function definitions */
static void on_resubscribe(void *subidp) {
  int subid = *((int *)subidp);
  pubsub_resubscribe(subid);
}
static void on_ondata(struct subnet_conn *c, int sink, short subid, void *data) {
  if (sink == pubsub_myid() && on_reading != NULL) {
    on_reading(subid, data);
  }
}
/*---------------------------------------------------------------------------*/
/** @} */
