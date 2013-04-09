/**
 * \addtogroup pubsub
 * @{
 */

/**
 * \file   Base implementation file for the Publish-Subscribe library
 * \author Jon Gjengset <jon@tsp.io>
 */

#include "lib/pubsub/common.h"
#include <string.h>
/*---------------------------------------------------------------------------*/
/* private functions */
static enum existance sub_state(struct full_subscription *s);
static bool is_active(struct full_subscription *s);

static void on_subscribe(struct subnet_conn *c, int sink, short subid, void *data);
static void on_unsubscribe(struct subnet_conn *c, int sink, short subid);
static enum existance on_exists(struct subnet_conn *c, int sink, short subid);
static size_t on_inform(struct subnet_conn *c, int sink, short subid, void *target);
/*---------------------------------------------------------------------------*/
/* private members */
static struct full_subscription subscriptions[SUBNET_MAX_SINKS][PUBSUB_MAX_SUBSCRIPTIONS];
static struct full_subscription *past = &subscriptions[SUBNET_MAX_SINKS-1][PUBSUB_MAX_SUBSCRIPTIONS-1]+1;
static struct pubsub_state state;
static struct subnet_callbacks su = {
  NULL,
  NULL,
  NULL,
  on_subscribe,
  on_unsubscribe,
  on_exists,
  on_inform
};
/*---------------------------------------------------------------------------*/
/* public function definitions */
struct full_subscription * find_subscription(int sink, short subid) {
  return &subscriptions[sink][subid];
}

void pubsub_init(struct pubsub_callbacks *u) {
  /* pass-thru callbacks */
  su.errpub = u->on_errpub;
  su.ondata = u->on_ondata;
  su.onsent = u->on_onsent;

  /* all subscriptions are unknown/invalid initially */
  for (int i = 0; i < SUBNET_MAX_SINKS; i++) {
    for (int j = 0; j < PUBSUB_MAX_SUBSCRIPTIONS; j++) {
      subscriptions[i][j].sink = -1;
    }
  }

  /* store callbacks */
  state.u = u;

  /* and start subnet networking */
  subnet_open(&state.c, 14159, 26535, &su);
}

bool pubsub_next_subscription(struct full_subscription *sub) {
  if (sub == NULL) {
    /* no previous, start from beginning */
    sub = &subscriptions[0][0];
  } else {
    /* otherwise, move on */
    sub++;
  }

  /* find next active subscription or the end */
  while (sub != past && !is_active(sub)) {
    sub++;
  }

  /* if we didn't reach the end, we have a next! */
  return sub != past;
}

bool pubsub_add_data(int sinkid, short subid, void *payload, size_t bytes) {
  return subnet_add_data(&state.c, sinkid, subid, payload, bytes);
}
void pubsub_publish(int sinkid) {
  subnet_publish(&state.c, sinkid);
}
short pubsub_subscribe(struct subscription *s) {
  return subnet_subscribe(&state.c, s, sizeof(struct subscription));
}
void pubsub_unsubscribe(short subid) {
  subnet_unsubscribe(&state.c, subid);
}
int pubsub_myid() {
  return subnet_myid(&state.c);
}
/*---------------------------------------------------------------------------*/
/* private function definitions */
static enum existance sub_state(struct full_subscription *s) {
  if (s->sink == -1) {
    /* invalid (never started) subscription */
    return UNKNOWN;
  }

  if (s->revoked == 0) {
    /* known, non-revoked subscription */
    return KNOWN;
  }

  if (clock_seconds() - s->revoked < 600) {
    /* known, revoked, but not expired subscription */
    /* TODO: This should be adjustable */
    return REVOKED;
  }

  /* known, revoked and expired subscription */
  return UNKNOWN;
}

static bool is_active(struct full_subscription *s) {
  return sub_state(s) == KNOWN;
}

static void on_subscribe(struct subnet_conn *c, int sink, short subid, void *data) {
  struct full_subscription *s = find_subscription(sink, subid);
  s->subid = subid;
  s->sink = sink;
  s->revoked = 0;
  memcpy(&s->in, data, sizeof(struct subscription));

  if (state.u->on_subscription != NULL) {
    state.u->on_subscription(s);
  }
}

static void on_unsubscribe(struct subnet_conn *c, int sink, short subid) {
  struct full_subscription *remove = find_subscription(sink, subid);
  if (remove->revoked == 0) {
    remove->revoked = clock_seconds();

    if (state.u->on_unsubscription != NULL) {
      state.u->on_unsubscription(remove);
    }
  }
}

static enum existance on_exists(struct subnet_conn *c, int sink, short subid) {
  struct full_subscription *s = find_subscription(sink, subid);
  return sub_state(s);
}

static size_t on_inform(struct subnet_conn *c, int sink, short subid, void *target) {
  struct full_subscription *s = find_subscription(sink, subid);

  /* TODO: Verify that PACKETBUF_SIZE is the right value to use here */
  if (packetbuf_datalen() + sizeof(struct subscription) > PACKETBUF_SIZE) {
    return 0;
  }

  memcpy(target, &s->in, sizeof(struct subscription));
  return sizeof(struct subscription);
}
/*---------------------------------------------------------------------------*/

/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
