/**
 * \addtogroup pubsub
 * @{
 */

/**
 * \file   Base implementation file for the Publish-Subscribe library
 * \author Jon Gjengset <jon@tsp.io>
 */

#include "lib/pubsub.h"
#include "sys/ctimer.h"
#include <string.h>

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
/* private functions */
static enum existance sub_state(struct full_subscription *s);

static void on_errpub(struct subnet_conn *c);
static void on_ondata(struct subnet_conn *c, int sink, subid_t subid, void *data);
static void on_onsent(struct subnet_conn *c, int sink, subid_t subid);
static void on_subscribe(struct subnet_conn *c, int sink, subid_t subid, void *data);
static void on_unsubscribe(struct subnet_conn *c, int sink, subid_t subid);
static enum existance on_exists(struct subnet_conn *c, int sink, subid_t subid);
static dlen_t on_inform(struct subnet_conn *c, int sink, subid_t subid, void *target);
static void on_sink_left(struct subnet_conn *c, int sink);
/*---------------------------------------------------------------------------*/
/* private members */
struct sink_subscriptions {
  subid_t maxsub;
  struct full_subscription subs[PUBSUB_MAX_SUBSCRIPTIONS];
};
static struct sink_subscriptions sinks[SUBNET_MAX_SINKS];
static struct pubsub_state state;
static struct subnet_callbacks su = {
  on_errpub,
  on_ondata,
  on_onsent,
  on_subscribe,
  on_unsubscribe,
  on_exists,
  on_inform,
  on_sink_left
};
/*---------------------------------------------------------------------------*/
/* public function definitions */
struct full_subscription * find_subscription(int sink, subid_t subid) {
  return &sinks[sink].subs[subid];
}

int last_subscription(int sink) {
  return sinks[sink].maxsub;
}

bool is_active(struct full_subscription *s) {
  if (s == NULL) {
    return false;
  }
  return sub_state(s) == KNOWN;
}

void pubsub_init(struct pubsub_callbacks *u) {
  int i, j;
  /* all subscriptions are unknown/invalid initially */
  for (i = 0; i < SUBNET_MAX_SINKS; i++) {
    sinks[i].maxsub = 0;
    for (j = 0; j < PUBSUB_MAX_SUBSCRIPTIONS; j++) {
      sinks[i].subs[j].sink = i;
      sinks[i].subs[j].subid = j;
      sinks[i].subs[j].revoked = 1;
    }
  }

  /* store callbacks */
  state.u = u;

  /* and start subnet networking */
  subnet_open(&state.c, 14159, 26535, &su);
}

bool pubsub_next_subscription(struct full_subscription **sub) {
  int sink;
  subid_t subid;

  if (*sub == NULL) {
    /* start from beginning */
    *sub = &sinks[0].subs[0];
    sink = 0;
    subid = 0;
    if (is_active(*sub)) return true;
  }

  /* find next active subscription or the end */
  do {
    sink = (*sub)->sink;
    subid = (*sub)->subid;
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
  } while (!is_active(*sub));

  return true;
}

bool pubsub_add_data(int sinkid, subid_t subid, void *payload, dlen_t bytes) {
  return subnet_add_data(&state.c, sinkid, subid, payload, bytes);
}
void pubsub_publish(int sinkid) {
  subnet_publish(&state.c, sinkid);
}
subid_t pubsub_subscribe(struct subscription *s) {
  return subnet_subscribe(&state.c, s, sizeof(struct subscription));
}
void pubsub_resubscribe(subid_t subid) {
  struct subscription *s = &find_subscription(pubsub_myid(), subid)->in;
  return subnet_resubscribe(&state.c, subid, s, sizeof(struct subscription));
}
void pubsub_unsubscribe(subid_t subid) {
  subnet_unsubscribe(&state.c, subid);
}
int pubsub_myid() {
  return subnet_myid(&state.c);
}
void pubsub_close() {
  subnet_close(&state.c);
}
int extract_data(struct full_subscription *sub, void *payloads[]) {
  /* TODO: this is not clean separation of concerns - it's a dirty dirty hack */
  const struct sink *s = subnet_sink(&state.c, sub->sink);
  int num = 0;

  EACH_SINK_FRAGMENT(s,
    PRINTF("pubsub: hit value for %d with size %d\n", subid, frag->length);
    if (subid != sub->subid) continue;
    if (frag->length == 0) continue;
    PRINTF("pubsub: extracted.\n");
    payloads[num] = payload;
    num++;
  );

  return num;
}
void pubsub_writeout(int sinkid) {
  subnet_writeout(&state.c, sinkid);
}
void pubsub_writein() {
  subnet_writein(&state.c);
}
/*---------------------------------------------------------------------------*/
/* private function definitions */
static void on_errpub(struct subnet_conn *c) {
  if (state.u->on_errpub != NULL) {
    state.u->on_errpub();
  }
}

static void on_ondata(struct subnet_conn *c, int sink, subid_t subid, void *data) {
  if (state.u->on_ondata != NULL) {
    state.u->on_ondata(sink, subid, data);
  }
}

static void on_onsent(struct subnet_conn *c, int sink, subid_t subid) {
  if (state.u->on_onsent != NULL) {
    state.u->on_onsent(sink, subid);
  }
}

static enum existance sub_state(struct full_subscription *s) {
  if (s->revoked == 1) {
    /* invalid (never started) subscription */
    return UNKNOWN;
  }

  if (s->revoked == 0) {
    /* known, non-revoked subscription */
    return KNOWN;
  }

  if (clock_seconds() - s->revoked < SUBNET_REVOKE_PERIOD) {
    /* known, revoked, but not expired subscription */
    return REVOKED;
  }

  /* known, revoked and expired subscription */
  return UNKNOWN;
}

static void on_subscribe(struct subnet_conn *c, int sink, subid_t subid, void *data) {
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

static void on_unsubscribe(struct subnet_conn *c, int sink, subid_t subid) {
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

static enum existance on_exists(struct subnet_conn *c, int sink, subid_t subid) {
  struct full_subscription *s = find_subscription(sink, subid);
  return sub_state(s);
}

static dlen_t on_inform(struct subnet_conn *c, int sink, subid_t subid, void *target) {
  struct full_subscription *s = find_subscription(sink, subid);

  if (packetbuf_datalen() + sizeof(struct subscription) > PACKETBUF_SIZE) {
    return 0;
  }

  memcpy(target, &s->in, sizeof(struct subscription));
  return sizeof(struct subscription);
}

static void on_sink_left(struct subnet_conn *c, int sink) {
  struct sink_subscriptions s = sinks[sink];
  subid_t i;

  for (i = 0; i <= s.maxsub; i++) {
    if (s.subs[i].revoked == 0) {
      s.subs[i].revoked = clock_seconds();
    }
  }
  s.maxsub = 0;
}
/*---------------------------------------------------------------------------*/

/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
