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
struct sink_subscriptions {
  short maxsub;
  struct full_subscription subs[PUBSUB_MAX_SUBSCRIPTIONS];
};
static struct sink_subscriptions sinks[SUBNET_MAX_SINKS];
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
  return &sinks[sink].subs[subid];
}

void pubsub_init(struct pubsub_callbacks *u) {
  /* pass-thru callbacks */
  su.errpub = u->on_errpub;
  su.ondata = u->on_ondata;
  su.onsent = u->on_onsent;

  /* all subscriptions are unknown/invalid initially */
  for (int i = 0; i < SUBNET_MAX_SINKS; i++) {
    sinks[i].maxsub = 0;
    for (int j = 0; j < PUBSUB_MAX_SUBSCRIPTIONS; j++) {
      sinks[i].subs[j].sink = -1;
    }
  }

  /* store callbacks */
  state.u = u;

  /* and start subnet networking */
  subnet_open(&state.c, 14159, 26535, &su);
}

bool pubsub_next_subscription(struct full_subscription **sub) {
  int sink;
  short subid;

  /* find next active subscription or the end */
  do {
    if (*sub == NULL) {
      /* start from beginning */
      *sub = &sinks[0].subs[0];
      sink = 0;
      subid = 0;
    } else {
      if (subid >= sinks[sink].maxsub) {
        sink++;

        if (sink == SUBNET_MAX_SINKS) {
          *sub = NULL;
          return false;
        }

        subid = 0;
      } else {
        subid++;
      }

      *sub = &sinks[sink].subs[subid];
    }
  } while (!is_active(*sub));

  return true;
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
void pubsub_resubscribe(short subid) {
  struct subscription *s = &find_subscription(pubsub_myid(), subid)->in;
  return subnet_resubscribe(&state.c, subid, s, sizeof(struct subscription));
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
  if (s == NULL) {
    return false;
  }
  return sub_state(s) == KNOWN;
}

static void on_subscribe(struct subnet_conn *c, int sink, short subid, void *data) {
  struct full_subscription *s = find_subscription(sink, subid);
  s->subid = subid;
  s->sink = sink;
  s->revoked = 0;
  memcpy(&s->in, data, sizeof(struct subscription));

  if (sinks[sink].maxsub < subid) {
    sinks[sink].maxsub = subid;
  }

  if (state.u->on_subscription != NULL) {
    state.u->on_subscription(s);
  }
}

static void on_unsubscribe(struct subnet_conn *c, int sink, short subid) {
  struct full_subscription *remove = find_subscription(sink, subid);
  if (remove->revoked == 0) {
    if (sinks[sink].maxsub == subid) {
      /* This could be changed to find next highest, but... */
      sinks[sink].maxsub = subid-1;
    }

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
