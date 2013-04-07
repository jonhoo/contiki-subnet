/**
 * \addtogroup pubsub
 * @{
 */

/**
 * \file   Base implementation file for the Publish-Subscribe library
 * \author Jon Gjengset <jon@tsp.io>
 */

#include "lib/pubsub/common.h"
/*---------------------------------------------------------------------------*/
/* private members */
static struct full_subscription *subscriptions;
static int numsubscriptions;
static struct pubsub_state *state;
/*---------------------------------------------------------------------------*/
/* private functions */
static void on_subscribe(struct subnet_conn *c, int sink, short subid, void *data);
static void on_unsubscribe(struct subnet_conn *c, int sink, short subid)
static bool on_exists(struct subnet_conn *c, int sink, short subid);
static size_t on_inform(struct subnet_conn *c, int sink, short subid, void *target);
/*---------------------------------------------------------------------------*/
/* public function definitions */
struct full_subscription * find_subscription(int sink, short subid) {
  struct full_subscription *s;
  for (int i = 0; i < numsubscriptions; i++) {
    s = &subscriptions[i];
    if (s->subid != subid) {
      continue;
    }
    if (s->sink == sink) {
      return s;
    }
  }

  return NULL;
}

void pubsub_init(struct pubsub_callbacks *u) {
  static struct full_subscription[PUBSUB_MAX_SUBSCRIPTIONS] ss;
  static struct pubsub_state s;
  static struct subnet_callbacks u = {
    u->on_errpub,
    u->on_ondata,
    u->on_onsent,
    on_subscribe,
    on_unsubscribe,
    on_exists,
    on_inform
  };

  subscriptions = &ss;
  state = &s;

  subnet_open(&state->c, 14159, 26535, &u);
}

int pubsub_get_subscriptions(struct full_subscription **ss) {
  *s = subscriptions;
  return numsubscriptions;
}

bool pubsub_add_data(int sinkid, short subid, void *payload, size_t bytes) {
  return subnet_add_data(s->c, sinkid, subid, payload, bytes);
}
void pubsub_publish(int sinkid) {
  subnet_publish(s->c, sinkid);
}
short pubsub_subscribe(void *payload, size_t bytes) {
  return subnet_subscribe(s->c, payload, bytes);
}
void pubsub_unsubscribe(short subid) {
  subnet_unsubscribe(s->c, subid);
}
/*---------------------------------------------------------------------------*/
/* private function definitions */
static void on_subscribe(struct subnet_conn *c, int sink, short subid, void *data) {
  if (numsubscriptions == PUBSUB_MAX_SUBSCRIPTIONS) {
    PRINTF("Subscription buffer full!");
    return;
  }

  struct full_subscription *s = &subscriptions[numsubscriptions];
  s->subid = subid;
  s->sink = sink;
  memcpy(&s->in, data, sizeof(struct subscription));
  numsubscriptions++;

  if (c->u->on_subscription != NULL) {
    c->u->on_subscription(s);
  }
}

static void on_unsubscribe(struct subnet_conn *c, int sink, short subid) {
  struct full_subscription *remove = find_subscription(sink, subid);
  if (remove == NULL) {
    return;
  }

  if (c->u->on_unsubscription != NULL) {
    c->u->on_unsubscription(remove);
  }

  if (numsubscriptions > 1) {
    struct full_subscription *last = &subscriptions[numsubscriptions-1];
    memcpy(remove, last, sizeof(struct full_subscription));
  }
  numsubscriptions--;
}

static bool on_exists(struct subnet_conn *c, int sink, short subid) {
  return find_subscription(sink, subid) != NULL;
}

static size_t on_inform(struct subnet_conn *c, int sink, short subid, void *target) {
  struct full_subscription *s = find_subscription(sink, subid);
  if (s == NULL) {
    return 0;
  }

  /* TODO: Verify that PACKETBUF_SIZE is the right value to use here */
  if (packetbuf_datalen() + sizeof(struct subscription) > PACKETBUF_SIZE) {
    return 0;
  }

  memcpy(target, s->in, sizeof(struct subscription));
  return sizeof(struct subscription);
}
/*---------------------------------------------------------------------------*/

/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
