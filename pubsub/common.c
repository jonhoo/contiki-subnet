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
static void on_subscribe(struct subnet_conn *c, const rimeaddr_t *sink, short subid, void *data);
static void on_unsubscribe(struct subnet_conn *c, const rimeaddr_t *sink, short subid)
static bool on_exists(struct subnet_conn *c, const rimeaddr_t *sink, short subid);
static size_t on_inform(struct subnet_conn *c, const rimeaddr_t *sink, short subid, void *target);
/*---------------------------------------------------------------------------*/
/* public function definitions */
struct subscription * find_subscription(const rimeaddr_t *sink, short subid) {
  struct full_subscription *s;
  for (int i = 0; i < numsubscriptions; i++) {
    s = &subscriptions[i];
    if (s->subid != subid) {
      continue;
    }
    if (rimeaddr_cmp(&s->sink, sink)) {
      return s;
    }
  }

  return NULL;
}

void pubsub_init(
  void (* on_errpub)(struct subnet_conn *c),
  void (* on_ondata)(struct subnet_conn *c, const rimeaddr_t *sink, short subid, void *data),
  void (* on_onsent)(struct subnet_conn *c, const rimeaddr_t *sink, short subid),
  void (* on_change)()
  )
{
  static struct full_subscription[PUBSUB_MAX_SUBSCRIPTIONS] ss;
  static struct pubsub_state s;
  static struct subnet_callbacks u = {
    on_errpub,
    on_ondata,
    on_onsent,
    on_subscribe,
    on_unsubscribe,
    on_exists,
    on_inform
  };

  subscriptions = &ss;
  state = &s;
  state->on_change = onchange;

  subnet_open(&state->c, 14159, 26535, &u);
}

int get_subscriptions(struct full_subscription **ss) {
  *s = subscriptions;
  return numsubscriptions;
}
/*---------------------------------------------------------------------------*/
/* private function definitions */
static void on_subscribe(struct subnet_conn *c, const rimeaddr_t *sink, short subid, void *data) {
  if (numsubscriptions == PUBSUB_MAX_SUBSCRIPTIONS) {
    PRINTF("Subscription buffer full!");
    return;
  }

  struct full_subscription *s = &subscriptions[numsubscriptions];
  s->subid = subid;
  rimeaddr_copy(&s->sink, sink);
  memcpy(&s->in, data, sizeof(struct subscription));
  numsubscriptions++;

  /* TODO: find a way of batching on_change calls */
  if (state->on_change != NULL) {
    state->on_change();
  }
}

static void on_unsubscribe(struct subnet_conn *c, const rimeaddr_t *sink, short subid) {
  struct full_subscription *remove = find_subscription(sink, subid);
  if (remove == NULL) {
    return;
  }

  if (numsubscriptions > 1) {
    struct full_subscription *last = &subscriptions[numsubscriptions-1];
    memcpy(remove, last, sizeof(struct full_subscription));
  }
  numsubscriptions--;

  if (state->on_change != NULL) {
    state->on_change();
  }
}

static bool on_exists(struct subnet_conn *c, const rimeaddr_t *sink, short subid) {
  return find_subscription(sink, subid) != NULL;
}

static size_t on_inform(struct subnet_conn *c, const rimeaddr_t *sink, short subid, void *target) {
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
