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

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
#define EACH_FRAGMENT(BLOCK) \
  { \
    int fragi;                                                   \
    short subid;                                                 \
    short fragments = packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS); \
    struct fragment *frag = packetbuf_dataptr();                 \
    void *payload;                                               \
                                                                 \
    for (fragi = 0; fragi < fragments; fragi++) {                \
      subid = next_fragment(&frag, &payload);                    \
      BLOCK \
    } \
  }
#define SKIPBYTES(VAR, TYPE, BYTES) \
  /* note the cast to char* to be able to move in bytes */ \
  VAR = (TYPE)(((char*) VAR)+BYTES);
/*---------------------------------------------------------------------------*/
static uint8_t get_advertised_cost(struct subnet_conn *c, const rimeaddr_t *sink) {
  for (i = 0; i < c->numroutes; i++) {
    if (rimeaddr_cmp(&c->routes[i].sink, sink)) {
      return c->routes[i].advertised_cost;
    }
  }

  return 0;
}
static const rimeaddr_t* get_next_hop(struct subnet_conn *c, const rimeaddr_t *sink, const rimeaddr_t *prevto) {
  int i;
  struct neighbor *n = NULL;
  struct sink_route *route = NULL;
  struct neighbor *next;

  /* find neighbor pointer */
  if (prevto != NULL) {
    for (i = 0; i < c->numneighbors; i++) {
      if (rimeaddr_cmp(&c->neighbors[i].addr, prevto)) {
        n = &c->neighbors[i];
      }
    }
  }

  /* find sink route */
  for (i = 0; i < c->numroutes; i++) {
    if (rimeaddr_cmp(&c->routes[i].sink, sink)) {
      route = &c->routes[i]
    }
  }

  /* find next most expensive route after n */
  next = n;
  for (i = 0; i < route.numhops; i++) {
    if (next == NULL || route.nexthops[i]->cost > next.cost) {
      /* TODO: Deal with multiple next hops with the same cost */
      /* TODO: Handle link quality? */
      next = route.nexthops[i];
    }
  }

  if (next == n || next == NULL) {
    /* no next route found */
    return NULL;
  }

  return n->addr;
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
short next_fragment(struct fragment **raw, void **payload) {
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
/*---------------------------------------------------------------------------*/
void update_routes(struct subnet_conn *c, rimeaddr_t *sink, rimeaddr_t *from) {
  int i;
  struct neighbor *n = NULL;
  struct sink_route *route = NULL;
  struct neighbor *cheap;
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
  for (i = 0; i < c->numroutes; i++) {
    if (rimeaddr_cmp(&c->routes[i].sink, sink)) {
      route = &c->routes[i]
    }
  }

  /* create sink node if not found */
  if (route == NULL) {
    if (c->numroutes >= SUBNET_MAX_SINKS) {
      PRINTF("%d.%d: subnet: max sinks limit hit\n",
          rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1]);
      return;
    } else {
      route = &c->routes[c->numroutes];
      rimeaddr_copy(&route->sink, sink);
      route->advertised_cost = n->cost + 1;
      route->numhops = 0;
      c->numroutes++;
    }
  }

  /* find cheapest and oldest next hop towards sink */
  cheap = n;
  oldest = n;
  for (i = 0; i < route->numhops; i++) {
    if (rimeaddr_cmp(&route->nexthops[i]->addr, from)) {
      n = NULL;
    }

    if (route->nexthops[i]->cost < cheapest->cost) {
      cheapest = &route->nexthops[i];
    }

    if (route->nexthops[i]->last_active < oldest->last_active) {
      oldest = &route->nexthops[i];
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

void handle_subscriptions(struct subnet_conn *c, const rimeaddr_t *sink, const rimeaddr_t *from) {
  bool unsubscribe = (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_UNSUBSCRIBE);

  if (c->u->exists == NULL) {
    return;
  }

  update_routes(c, sink, from);

  EACH_FRAGMENT(
    if (!!c->u->exists(c, sink, subid) == unsubscribe) {
      /* something changed, send new subscription to neighbours */
      broadcast(&c->pubsub);
      break;
    }
  );

  if (c->u->subscribe == NULL) {
    return;
  }

  EACH_FRAGMENT(
    if (!!c->u->exists(c, sink, subid) == unsubscribe) {
      if (unsubscribe) {
        c->u->unsubscribe(c, sink, subid);
      } else {
        c->u->subscribe(c, sink, subid, payload);
      }
    }
  );
}
/*---------------------------------------------------------------------------*/
static void on_peer(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno) {
  /* peer points to second adisclose_conn, so we need to decrement to cast to
   * subnet_conn */
  struct subnet_conn *c = (struct subnet_conn *)(adisclose-1);
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);

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
        frag->length = c->u->inform(c, sink, subids[i], frag+1);

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

  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_REPLY) {
    handle_subscriptions(c, sink, from);
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

  if (c->u->ondata == NULL) {
    return;
  }

  EACH_FRAGMENT(
    c->u->ondata(c, sink, subid, payload);
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

  if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_SUBSCRIBE) {
    handle_subscriptions(c, sink, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_UNSUBSCRIBE) {
    handle_subscriptions(c, sink, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_PUBLISH) {

    if (c->u->exists == NULL) {
      return;
    }

    short fragments = packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS);
    short unknowns[fragments];
    short numunknowns = 0;
    EACH_FRAGMENT(
      if (!c->u->exists(c, sink, subid)) {
        unknowns[numunknowns] = subid;
        numunknowns++;
      }
    );

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
static void on_timedout(struct adisclose_conn *c, const rimeaddr_t *to) {
  const rimeaddr_t *nexthop = get_next_hop(c, packetbuf_addr(PACKETBUF_ADDR_ERECEIVER), to);

  if (nexthop == NULL) {
    queuebuf_free(c->sentpacket);
    c->sentpacket = NULL;

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

  if (c->sentpacket != NULL) {
    queuebuf_free(c->sentpacket);
    c->sentpacket = NULL;
  }

  if (c->u->onsent == NULL) {
    return;
  }

  EACH_FRAGMENT(
    c->u->onsent(c, sink, subid);
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
  c->numroutes = 0;
}

void subnet_close(struct subnet_conn *c) {
  adisclose_close(&c->pubsub);
  adisclose_close(&c->peer);
}

void subnet_prepublish(struct subnet_conn *c, const rimeaddr_t *sink) {
  /* preserve existing packetbuf */
  struct queuebuf *q = queuebuf_new_from_packetbuf();

  packetbuf_clear();
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_PUBLISH);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, sink);
  packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, 0);
  c->fragments = 0;
  c->frag = packetbuf_dataptr();
  packetbuf_set_datalen(0);

  /* store incomplete publish packet */
  c->sentpacket = queuebuf_new_from_packetbuf();

  /* restore previous packetbuf */
  queuebuf_to_packetbuf(q);
  queuebuf_free(q);
}

bool subnet_add_data(struct subnet_conn *c, short subid, void *payload, size_t bytes) {
  /* preserve existing packetbuf */
  struct queuebuf *q = queuebuf_new_from_packetbuf();
  /* restore publish packet */
  queuebuf_to_packetbuf(c->sentpacket);
  queuebuf_free(c->sentpacket); /* why doesn't contiki have a queuebuf_update */

  uint16_t l = packetbuf_totlen();
  size_t sz = sizeof(struct fragment) + bytes;
  if (l + sz > PACKETBUF_SIZE) {
    /* restore old packetbuf */
    queuebuf_to_packetbuf(q);
    queuebuf_free(q);
    return false;
  }

  c->frag->subid = subid;
  c->frag->length = bytes;
  c->frag++;
  c->fragments++;
  memcpy(c->frag, payload, bytes);
  SKIPBYTES(c->frag, struct fragment *, bytes);
  packetbuf_set_datalen(packetbuf_datalen() + sz);

  /* store publish packet and restore old packetbuf */
  c->sentpacket = queuebuf_new_from_packetbuf();
  queuebuf_to_packetbuf(q);
  queuebuf_free(q);

  return true;
}

void subnet_publish(struct subnet_conn *c) {
  /* preserve existing packetbuf */
  struct queuebuf *q = queuebuf_new_from_packetbuf();
  /* restore publish packet */
  queuebuf_to_packetbuf(c->sentpacket);
  queuebuf_free(c->sentpacket);

  packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, c->fragments);
  const rimeaddr_t *nexthop = get_next_hop(c, packetbuf_addr(PACKETBUF_ADDR_ERECEIVER), NULL);
  if (nexthop == NULL || adisclose_is_transmitting(c)) {
    /* restore old packetbuf */
    queuebuf_to_packetbuf(q);
    queuebuf_free(q);

    if (c->u->errpub != NULL) {
      c->u->errpub(c);
    }
    return;
    /* TODO: let upstream know whether we failed because no next hop or busy */
  }
  packetbuf_set_attr(PACKETBUF_ATTR_HOPS, get_advertised_cost(c, sink));

  adisclose_send(&c->pubsub, nexthop);

  /* store publish packet and restore old packetbuf */
  c->sentpacket = queuebuf_new_from_packetbuf();
  queuebuf_to_packetbuf(q);
  queuebuf_free(q);
}

short subnet_subscribe(struct subnet_conn *c, void *payload, size_t bytes) {
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
  return c->subid;
}

void subnet_unsubscribe(struct subnet_conn *c, short subid) {
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
}
/** @} */
