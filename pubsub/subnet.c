/**
 * \addtogroup subnet
 * @{
 */


/**
 * \file
 *         Subnet networking protocol
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#include "net/rime/subnet.h"
#include "net/rime.h"
#include "net/rime/disclose.h"
#include <string.h>

static const struct packetbuf_attrlist attributes[] = {
    SUBNET_ATTRIBUTES
    PACKETBUF_ATTR_LAST
  };

#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
#define EACH_FRAGMENT(FRAGMENTS, BUF, BLOCK) \
  { \
    int fragi;                                                   \
    short subid;                                                 \
    short fragments = FRAGMENTS;                                 \
    struct fragment *frag = (struct fragment *) BUF;             \
    void *payload;                                               \
                                                                 \
    for (fragi = 0; fragi < fragments; fragi++) {                \
      subid = next_fragment(&frag, &payload);                    \
      BLOCK \
    } \
  }

#define EACH_SINK_FRAGMENT(S, BLOCK) \
  EACH_FRAGMENT(S->fragments, S->buf, BLOCK)

#define EACH_PACKET_FRAGMENT(BLOCK) \
  EACH_FRAGMENT(packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS),      \
                packetbuf_dataptr(),                            \
                BLOCK)

#define SKIPBYTES(VAR, TYPE, BYTES) \
  /* note the cast to char* to be able to move in bytes */       \
  VAR = (TYPE)(((char*) VAR)+BYTES);
/*---------------------------------------------------------------------------*/
static int find_sinkid(struct subnet_conn *c, const rimeaddr_t *sink) {
  for (int i = 0; i < c->numsinks; i++) {
    if (rimeaddr_cmp(&c->sinks[i].sink, sink)) {
      return i;
    }
  }

  return -1;
}
static struct sink *find_sink(struct subnet_conn *c, const rimeaddr_t *sink) {
  int sinkid = find_sinkid(c, sink);
  if (sinkid == -1) {
    return NULL;
  }
  return &c->sinks[sinkid];
}
static uint8_t get_advertised_cost(struct subnet_conn *c, const rimeaddr_t *sink) {
  struct sink *s = find_sink(c, sink);
  if (s == NULL) {
    return 0;
  }

  return s->advertised_cost;
}
static const rimeaddr_t* get_next_hop(struct subnet_conn *c, struct sink *route, const rimeaddr_t *prevto) {
  int i;
  struct neighbor *n = NULL;
  struct neighbor *next;

  if (route == NULL) {
    return NULL;
  }

  /* find neighbor pointer */
  if (prevto != NULL) {
    for (i = 0; i < c->numneighbors; i++) {
      if (rimeaddr_cmp(&c->neighbors[i].addr, prevto)) {
        n = &c->neighbors[i];
      }
    }
  }

  /* find next most expensive route after n */
  next = n;
  for (i = 0; i < route->numhops; i++) {
    if (next == NULL || route->nexthops[i]->cost > next->cost) {
      /* TODO: Deal with multiple next hops with the same cost */
      /* TODO: Handle link quality? */
      next = route->nexthops[i];
    }
  }

  if (next == n || next == NULL) {
    /* no next route found */
    return NULL;
  }

  return &n->addr;
}

static void broadcast(struct adisclose_conn *c) {
  /* note that we can use disclose here since we don't need any callbacks,
   * adisclose has no disclose callback on send and adisclose_conn has
   * disclose_conn as its first member. We use disclose rather than adisclose
   * since this is a broadcast and we don't want to wait for ACKs */
  disclose_send((struct disclose_conn *) c, &rimeaddr_node_addr);
}

/*
 * This function deserves some explanation.
 * It takes a pointer to a fragment pointer and a pointer to a payload pointer.
 * The former should initially point to the first data byte in a packet, and the
 * latter to an empty void pointer.
 * After being called, the first pointer will point to the next fragment, and
 * payload will point to the data contained in this fragment.
 * This function should only be called as many times as there are fragments in
 * the packet.
 */
static short next_fragment(struct fragment **raw, void **payload) {
  short subid = (*raw)->subid;
  size_t length = (*raw)->length;
  /* move past subid + length */
  *raw = *raw + 1;
  /* we're now at the payload */
  *payload = *raw;
  /* move past (length of payload) bytes */
  SKIPBYTES(*raw, struct fragment *, length);
  return subid;
}

static void update_routes(struct subnet_conn *c, const rimeaddr_t *sink, const rimeaddr_t *from) {
  int i;
  struct neighbor *n = NULL;
  struct sink *route = NULL;
  struct neighbor *oldest;
  int oldesti;

  /* find neighbor pointer */
  oldest = n;
  for (i = 0; i < c->numneighbors; i++) {
    if (rimeaddr_cmp(&c->neighbors[i].addr, from)) {
      n = &c->neighbors[i];
      break;
    }

    if (oldest == NULL || c->neighbors[i].last_active < oldest->last_active) {
      oldest = &c->neighbors[i];
    }
  }

  /* create neighbor if not found */
  if (n == NULL) {
    if (c->numneighbors >= SUBNET_MAX_NEIGHBORS) {
      PRINTF("%d.%d: subnet: max neighbours limit hit\n",
          rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1]);
      n = oldest;
    } else {
      n = &c->neighbors[c->numneighbors];
      c->numneighbors++;
    }

    rimeaddr_copy(&n->addr, from);
    n->last_active = clock_seconds();
    n->cost = packetbuf_attr(PACKETBUF_ATTR_HOPS);
  }

  /* find route to sink */
  for (i = 0; i < c->numsinks; i++) {
    if (rimeaddr_cmp(&c->sinks[i].sink, sink)) {
      route = &c->sinks[i];
    }
  }

  /* create sink node if not found */
  if (route == NULL) {
    if (c->numsinks >= SUBNET_MAX_SINKS) {
      PRINTF("%d.%d: subnet: max sinks limit hit\n",
          rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1]);
      return;
    } else {
      route = &c->sinks[c->numsinks];
      rimeaddr_copy(&route->sink, sink);
      route->advertised_cost = n->cost + 1;
      route->numhops = 0;
      route->buflen = 0;
      route->fragments = 0;
      c->numsinks++;
    }
  }

  /* find cheapest and oldest next hop towards sink */
  oldest = n;
  for (i = 0; i < route->numhops; i++) {
    if (rimeaddr_cmp(&route->nexthops[i]->addr, from)) {
      n = NULL;
    }

    if (route->nexthops[i]->last_active < oldest->last_active) {
      oldest = route->nexthops[i];
      oldesti = i;
    }
  }

  /* if *from was not stored as a next hop for sink and it is indeed a *next*
   * hop, add it (or replace with oldest) */
  if (n != NULL && n->cost <= route->advertised_cost) {
    if (route->numhops < SUBNET_MAX_ALTERNATE_ROUTES) {
      route->nexthops[route->numhops] = n;
      route->numhops++;
    /* } else if (oldest != n) { no need for this check since n is always new */
    } else {
      route->nexthops[oldesti] = n;
    }
  }
}

static void handle_subscriptions(struct subnet_conn *c, int sinkid, const rimeaddr_t *from) {
  bool unsubscribe = (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_UNSUBSCRIBE);

  if (c->u->exists == NULL) {
    return;
  }

  update_routes(c, &c->sinks[sinkid].sink, from);

  EACH_PACKET_FRAGMENT(
    if (!!c->u->exists(c, sinkid, subid) == unsubscribe) {
      /* something changed, send new subscription to neighbours */
      broadcast(&c->pubsub);
      break;
    }
  );

  if (c->u->subscribe == NULL) {
    return;
  }

  EACH_PACKET_FRAGMENT(
    if (!!c->u->exists(c, sinkid, subid) == unsubscribe) {
      if (unsubscribe) {
        c->u->unsubscribe(c, sinkid, subid);
      } else {
        c->u->subscribe(c, sinkid, subid, payload);
      }
    }
  );
}

static void clean_buffers(struct subnet_conn *c, struct sink *s) {
  if (c->sentpacket != NULL) {
    queuebuf_free(c->sentpacket);
    c->sentpacket = NULL;
  }

  if (s != NULL) {
    s->buflen = 0;
    s->fragments = 0;
  }
}
/*---------------------------------------------------------------------------*/
static void on_peer(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno) {
  /* peer points to second adisclose_conn, so we need to decrement to cast to
   * subnet_conn */
  struct subnet_conn *c = (struct subnet_conn *)(adisclose-1);
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);
  int sinkid = find_sinkid(c, sink);

  if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_ASK) {
    if (!rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &rimeaddr_node_addr)) {
      return;
    }

    if (c->u->inform == NULL) {
      return;
    }

    packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_REPLY);
    packetbuf_set_attr(PACKETBUF_ATTR_HOPS, get_advertised_cost(c, sink));
    {
      short fragments = packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS);
      struct fragment *frag = packetbuf_dataptr();
      void *payload;
      short subids[fragments];
      short *subid = packetbuf_dataptr();
      int i;

      for (i = 0; i < fragments; i++) {
        subids[i] = subid[i];
      }

      packetbuf_set_datalen(0);
      for (i = 0; i < fragments; i++) {
        packetbuf_set_datalen(packetbuf_datalen() + sizeof(struct fragment));
        frag->subid = subids[i];
        frag->length = c->u->inform(c, sinkid, subids[i], frag+1);

        if (frag->length == 0) {
          /* don't put an empty fragment in there */
          packetbuf_set_datalen(packetbuf_datalen() - sizeof(struct fragment));
          continue;
        }

        payload = (char *) frag;
        payload += frag->length + sizeof(struct fragment);
        frag = (struct fragment *) payload;
        packetbuf_set_datalen(packetbuf_datalen() + frag->length);
      }
    }

    /* packetbuf now holds info about subscription */
    adisclose_send(&c->peer, from);

  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_INVALIDATE) {
    handle_subscriptions(c, sinkid, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_REPLY) {
    handle_subscriptions(c, sinkid, from);
  }
}
/*---------------------------------------------------------------------------*/
/**
 * This is called when a downstream node sends a publish message to us as next
 * hop to forward towards the sink, so forward. Subscription must be known to us
 * here since otherwise we wouldn't be next hop.
 */
static void on_recv(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno) {
  struct subnet_conn *c = (struct subnet_conn *)adisclose;
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);
  int sinkid;

  if (c->u->ondata == NULL) {
    return;
  }

  sinkid = find_sinkid(c, sink);

  EACH_PACKET_FRAGMENT(
    c->u->ondata(c, sinkid, subid, payload);
  );
}

/**
 * Called when a neighbouring node sends a publish that is not destined for us,
 * or if we hear a subscribe packet from another node.
 * If it is a publish and the subscription is unknown, ask peer for it.
 * If it is a subscribe and the subscription is unknown, add subscription.
 */
static void on_hear(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno) {
  struct subnet_conn *c = (struct subnet_conn *)adisclose;
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);
  int sinkid = find_sinkid(c, sink);

  if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_SUBSCRIBE) {
    handle_subscriptions(c, sinkid, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_UNSUBSCRIBE) {
    handle_subscriptions(c, sinkid, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_PUBLISH) {

    if (c->u->exists == NULL) {
      return;
    }

    short fragments = packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS);
    short unknowns[fragments];
    short numunknowns = 0;
    EACH_PACKET_FRAGMENT(
      if (!c->u->exists(c, sinkid, subid)) {
        unknowns[numunknowns] = subid;
        numunknowns++;
      }
    );

    /* TODO: Send invalidate for known unsubscriptions */

    /* ask peer for clarification */
    struct queuebuf *q = queuebuf_new_from_packetbuf();

    packetbuf_clear();
    packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_ASK);
    packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, sink);
    packetbuf_copyfrom(&unknowns, numunknowns * sizeof(short));
    adisclose_send(&c->peer, from);

    queuebuf_to_packetbuf(q);
    queuebuf_free(q);
  }
}

/**
 * Called if a forwarding/sending failed. Should try next host in alternate list
 * or call errpub callback if no more alternates are available
 */
static void on_timedout(struct adisclose_conn *adisclose, const rimeaddr_t *to) {
  struct subnet_conn *c = (struct subnet_conn *)adisclose;
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);
  struct sink *s = find_sink(c, sink);
  const rimeaddr_t *nexthop = get_next_hop(c, s, to);

  if (nexthop == NULL) {
    clean_buffers(c, s);
    if (c->u->errpub != NULL) {
      c->u->errpub(c);
    }

    return;
  }

  queuebuf_to_packetbuf(c->sentpacket);
  adisclose_send(&c->pubsub, nexthop);
}

/**
 * Called if a forwarding/sending succeeded. Should result in an upstream
 * callback
 */
static void on_sent(struct adisclose_conn *adisclose, const rimeaddr_t *to) {
  struct subnet_conn *c = (struct subnet_conn *)adisclose;
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);
  int sinkid = find_sinkid(c, sink);
  struct sink *s = &c->sinks[sinkid];

  clean_buffers(c, s);
  if (c->u->onsent == NULL) {
    return;
  }

  EACH_SINK_FRAGMENT(s,
    c->u->onsent(c, sinkid, subid);
  );
}
/*---------------------------------------------------------------------------*/
static const struct adisclose_callbacks subnet = {
  on_recv,
  on_sent,
  on_timedout,
  on_hear,
};
static const struct adisclose_callbacks peer = {
  on_peer,
  NULL,
  NULL,
  on_peer,
};
/*---------------------------------------------------------------------------*/
void subnet_open(struct subnet_conn *c,
                 uint16_t subchannel,
                 uint16_t peerchannel,
                 const struct subnet_callbacks *u) {
  adisclose_open(&c->pubsub, subchannel, &subnet);
  adisclose_open(&c->peer, peerchannel, &peer);
  channel_set_attributes(subchannel, attributes);
  channel_set_attributes(peerchannel, attributes);
  c->u = u;
  c->subid = 0;
  c->numsinks = 0;
}

void subnet_close(struct subnet_conn *c) {
  adisclose_close(&c->pubsub);
  adisclose_close(&c->peer);
}

bool subnet_add_data(struct subnet_conn *c, int sinkid, short subid, void *payload, size_t bytes) {
  if (sinkid >= c->numsinks) {
    return false;
  }
  struct sink *s = &c->sinks[sinkid];

  uint16_t l = s->buflen;
  size_t sz = sizeof(struct fragment) + bytes;
  if (l + sz > PACKETBUF_SIZE) {
    return false;
  }

  struct fragment *f = (struct fragment *) (s->buf+s->buflen);
  f->subid = subid;
  f->length = bytes;

  s->buflen += sz;
  s->fragments++;
  memcpy(f+1, payload, bytes);

  return true;
}

void subnet_publish(struct subnet_conn *c, int sinkid) {
  if (sinkid >= c->numsinks) {
    return;
  }
  struct sink *s = &c->sinks[sinkid];

  const rimeaddr_t *nexthop = get_next_hop(c, s, NULL);
  if (nexthop == NULL || adisclose_is_transmitting(&c->pubsub)) {
    if (c->u->errpub != NULL) {
      c->u->errpub(c);
    }
    return;
    /* TODO: let upstream know whether we failed because no next hop or busy */
  }

  /* preserve existing packetbuf */
  struct queuebuf *q = queuebuf_new_from_packetbuf();

  packetbuf_clear();
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_PUBLISH);
  packetbuf_set_attr(PACKETBUF_ATTR_HOPS, get_advertised_cost(c, &s->sink));
  packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, s->fragments);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, &s->sink);
  memcpy(packetbuf_dataptr(), s->buf, s->buflen);

  adisclose_send(&c->pubsub, nexthop);

  /* store publish packet and restore old packetbuf */
  c->sentpacket = queuebuf_new_from_packetbuf();
  queuebuf_to_packetbuf(q);
  queuebuf_free(q);
}

short subnet_subscribe(struct subnet_conn *c, void *payload, size_t bytes) {
  struct queuebuf *q = queuebuf_new_from_packetbuf();

  packetbuf_clear();
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_SUBSCRIBE);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, &rimeaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, 1);
  packetbuf_set_attr(PACKETBUF_ATTR_HOPS, 0);

  struct fragment *f = packetbuf_dataptr();
  f->subid = ++c->subid;
  f->length = bytes;
  f++;
  memcpy(f, payload, bytes);
  packetbuf_set_datalen(sizeof(struct fragment) + bytes);

  broadcast(&c->pubsub);

  queuebuf_to_packetbuf(q);
  queuebuf_free(q);
  return c->subid;
}

void subnet_unsubscribe(struct subnet_conn *c, short subid) {
  struct queuebuf *q = queuebuf_new_from_packetbuf();

  packetbuf_clear();
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_UNSUBSCRIBE);
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, subid);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, &rimeaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_HOPS, 0);

  struct fragment *f = packetbuf_dataptr();
  f->subid = subid;
  f->length = 0;
  packetbuf_set_datalen(sizeof(struct fragment));

  broadcast(&c->pubsub);

  queuebuf_to_packetbuf(q);
  queuebuf_free(q);
}
/** @} */
