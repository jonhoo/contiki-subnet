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
/*---------------------------------------------------------------------------*/
/* public function definitions */
void subscriber_start(void (*cb)(short subid, void *data)) {
  on_reading = cb;
  pubsub_init(&callbacks);
}

short subscriber_subscribe(struct subscription *s) {
  return pubsub_subscribe(s);
}
short subscriber_replace(short subid, struct subscription *s) {
  pubsub_unsubscribe(subid);
  return pubsub_subscribe(s);
}
void subscriber_unsubscribe(short subid) {
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
static void on_ondata(struct subnet_conn *c, int sink, short subid, void *data) {
  if (sink == pubsub_myid() && on_reading != NULL) {
    on_reading(subid, data);
  }
}
/*---------------------------------------------------------------------------*/
/** @} */
