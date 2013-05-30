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
#define EACH_PACKET_FRAGMENT(BLOCK) \
  EACH_FRAGMENT(packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS),      \
                packetbuf_dataptr(),                            \
                BLOCK)

#define SKIPBYTES(VAR, TYPE, BYTES) \
  /* note the cast to char* to be able to move in bytes */       \
  VAR = (TYPE)(((char*) VAR)+BYTES);
/*---------------------------------------------------------------------------*/
/* private functions */
static short find_sinkid(struct subnet_conn *c, const rimeaddr_t *sink);
static const rimeaddr_t* get_next_hop(struct subnet_conn *c, struct sink *route, const rimeaddr_t *prevto);
static void broadcast(struct disclose_conn *c);
static bool is_known(struct subnet_conn *c, short sinkid, subid_t subid);
static void notify_left(struct subnet_conn *c, const rimeaddr_t *sink);
static void handle_leaving(struct subnet_conn *c, const rimeaddr_t *sink);
static void update_routes(struct subnet_conn *c, const rimeaddr_t *sink, const rimeaddr_t *from);
static void handle_subscriptions(struct subnet_conn *c, const rimeaddr_t *sink, const rimeaddr_t *from);
static bool inject_packetbuf(subid_t subid, dlen_t bytes, uint8_t *fragments, dlen_t *buflen, void *payload, void *buf);
static void prepare_packetbuf(uint8_t type, const rimeaddr_t *sink, uint8_t hops);

static void on_peer(struct disclose_conn *disclose, const rimeaddr_t *from);
static void on_recv(struct disclose_conn *disclose, const rimeaddr_t *from);
static void on_hear(struct disclose_conn *disclose, const rimeaddr_t *from);
static void on_sent(struct disclose_conn *disclose, int status);
/*---------------------------------------------------------------------------*/
/* private members */
static const struct disclose_callbacks subnet = {
  on_recv,
  on_hear,
  on_sent
};

static const struct disclose_callbacks peer = {
  on_peer,
  on_peer,
  NULL
};
/*---------------------------------------------------------------------------*/
/* public function definitions */
void subnet_open(struct subnet_conn *c,
                 uint16_t subchannel,
                 uint16_t peerchannel,
                 const struct subnet_callbacks *u) {
  disclose_open(&c->pubsub, subchannel, &subnet);
  disclose_open(&c->peer, peerchannel, &peer);
  channel_set_attributes(subchannel, attributes);
  channel_set_attributes(peerchannel, attributes);
  c->u = u;
  c->subid = 0;
  c->numsinks = 0;
  c->writeout = -1;
}

void subnet_close(struct subnet_conn *c) {
  prepare_packetbuf(SUBNET_PACKET_TYPE_LEAVING, &rimeaddr_node_addr, 0);

  /* TODO: perhaps do this multiple times for good measure? */
  broadcast(&c->pubsub);

  disclose_close(&c->pubsub);
  disclose_close(&c->peer);
}

bool subnet_add_data(struct subnet_conn *c, short sinkid, subid_t subid, void *payload, dlen_t bytes) {
  struct sink *s;
  PRINTF("subnet: adding data for %d:%d\n", sinkid, subid);

  if (sinkid >= c->numsinks) {
    PRINTF("subnet: invalid sink id\n");
    return false;
  }

  if (c->writeout == sinkid) {
    PRINTF("subnet: writing to writeout buffer!\n");
    s = &c->writesink;
  } else {
    s = &c->sinks[sinkid];
  }

  bool n = inject_packetbuf(subid, bytes, &s->fragments, &s->buflen, payload, s->buf+s->buflen);
  if (!n) {
    PRINTF("subnet: packet is full\n");
    return false;
  }

  return true;
}

void subnet_writeout(struct subnet_conn *c, short sinkid) {
  PRINTF("subnet: enabling writeout buffer\n");
  c->writeout = sinkid;
  c->writesink.fragments = 0;
  c->writesink.buflen = 0;
}

void subnet_writein(struct subnet_conn *c) {
  struct sink *s;
  PRINTF("subnet: disabling writeout buffer\n");
  if (c->writeout == -1) return;

  s = &c->sinks[c->writeout];
  s->fragments = c->writesink.fragments;
  s->buflen = c->writesink.buflen;
  memcpy(s->buf, c->writesink.buf, c->writesink.buflen);
  c->writeout = -1;
}

void subnet_publish(struct subnet_conn *c, short sinkid) {
  PRINTF("subnet: publish data\n");

  if (sinkid >= c->numsinks) {
    PRINTF("subnet: invalid sink id\n");
    return;
  }

  struct sink *s = &c->sinks[sinkid];
  const rimeaddr_t *nexthop = get_next_hop(c, s, NULL);

  if (nexthop == NULL) {
    PRINTF("subnet: no next hop known\n");

    if (c->u->errpub != NULL) {
      c->u->errpub(c);
    }
    return;
  }

  prepare_packetbuf(SUBNET_PACKET_TYPE_PUBLISH, &s->sink, s->advertised_cost);
  packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, s->fragments);
  memcpy(packetbuf_dataptr(), s->buf, s->buflen);
  packetbuf_set_datalen(s->buflen);

#if DEBUG
  PRINTF("subnet: publishing %d bytes to %d.%d via %d.%d\n",
      s->buflen,
      s->sink.u8[0], s->sink.u8[1],
      nexthop->u8[0], nexthop->u8[1]
      );

  EACH_PACKET_FRAGMENT(
    PRINTF("        fragment %d is %d bytes for %d...\n", fragi, frag->length, subid);
  );
#endif

  /* store publish packet */
  c->sentpacket = queuebuf_new_from_packetbuf();
  PRINTF("subnet: publish buffered\n");

  disclose_send(&c->pubsub, nexthop);

  PRINTF("subnet: publish sent to NIC, resetting\n");

  /* reset sink packetbuf */
  s->buflen = 0;
  s->fragments = 0;
}

subid_t subnet_subscribe(struct subnet_conn *c, void *payload, dlen_t bytes) {
  if (subnet_myid(c) == -1) {
    PRINTF("subnet: injecting sink into sink table\n");
    update_routes(c, &rimeaddr_node_addr, &rimeaddr_null);
  }

  subid_t subid = c->subid;
  subnet_resubscribe(c, subid, payload, bytes);
  c->subid++;
  return subid;
}

void subnet_resubscribe(struct subnet_conn *c, subid_t subid, void *payload, dlen_t bytes) {
  prepare_packetbuf(SUBNET_PACKET_TYPE_SUBSCRIBE, &rimeaddr_node_addr, 0);
  inject_packetbuf(subid, bytes, NULL, NULL, payload, NULL);

  if (!is_known(c, subnet_myid(c), subid)) {
    PRINTF("subnet: about to broadcast unknown subscription %d\n", subid);
    handle_subscriptions(c, &rimeaddr_node_addr, &rimeaddr_null);
    // handle_subscriptions will take care of the broadcast
  } else {
    PRINTF("subnet: re-broadcasting subscription %d\n", subid);
    broadcast(&c->pubsub);
  }
}

short subnet_packetlen(struct subnet_conn *c, short sinkid) {
  struct sink *s;

  if (sinkid >= c->numsinks) {
    PRINTF("subnet: invalid sink id\n");
    return -1;
  }

  if (c->writeout == sinkid) {
    s = &c->writesink;
  } else {
    s = &c->sinks[sinkid];
  }

  return s->buflen;
}

void subnet_unsubscribe(struct subnet_conn *c, subid_t subid) {
  prepare_packetbuf(SUBNET_PACKET_TYPE_UNSUBSCRIBE, &rimeaddr_node_addr, 0);
  inject_packetbuf(subid, 0, NULL, NULL, NULL, NULL);

  if (is_known(c, subnet_myid(c), subid)) {
    PRINTF("subnet: revoking subscription %d\n", subid);
    handle_subscriptions(c, &rimeaddr_node_addr, &rimeaddr_null);
    // handle_subscriptions will take care of the broadcast
  } else {
    PRINTF("subnet: re-broadcasting unsubscription for %d\n", subid);
    broadcast(&c->pubsub);
  }
}

short subnet_myid(struct subnet_conn *c) {
  static short myid = -1;
  if (myid == -1) {
    myid = find_sinkid(c, &rimeaddr_node_addr);
  }
  return myid;
}

struct fragment *next_fragment(struct fragment *frag, void **payload) {
  /* move past subid + length */
  struct fragment *next = frag + 1;
  /* we're now at the payload */
  *payload = next;
  /* move past (length of payload) bytes */
  SKIPBYTES(next, struct fragment *, frag->length);
  return next;
}

const struct sink *subnet_sink(struct subnet_conn *c, short sinkid) {
  return &c->sinks[sinkid];
}
/*---------------------------------------------------------------------------*/
/* private function definitions */
static short find_sinkid(struct subnet_conn *c, const rimeaddr_t *sink) {
  short i;
  for (i = 0; i < c->numsinks; i++) {
    if (rimeaddr_cmp(&c->sinks[i].sink, sink)) {
      return i;
    }
  }

  return -1;
}

static const rimeaddr_t* get_next_hop(struct subnet_conn *c, struct sink *route, const rimeaddr_t *prevto) {
  int i;
  int previ = -1;
  int nexti = -1;
  struct sink_neighbor *n = NULL;
  struct sink_neighbor *next;
  struct sink_neighbor *this;

  if (route == NULL) {
    PRINTF("subnet: cannot find next hop to unspecified sink\n");
    return NULL;
  }

  if (route->revoked > 0 && clock_seconds() - route->revoked > SUBNET_REVOKE_PERIOD) {
    PRINTF("subnet: sink revoked, pretending there is no known next hop\n");
    return NULL;
  }

  /* find neighbor pointer */
  if (prevto != NULL) {
    for (i = 0; i < route->numhops; i++) {
      if (rimeaddr_cmp(&route->nexthops[i].node->addr, prevto)) {
        PRINTF("subnet: found previous next hop neighbour entry\n");
        previ = i;
        n = &route->nexthops[i];
        break;
      }
    }
  }

  PRINTF("subnet: determining best next hop amongst:\n");

  /* find next most expensive route after n */
  next = n;
  for (i = 0; i < route->numhops; i++) {
    this = &route->nexthops[i];

    PRINTF("        %d.%d (cost: %d, last_active: %d)\n"
        , this->node->addr.u8[0]
        , this->node->addr.u8[1]
        , this->cost
        , (int) this->node->last_active);

    if (n != NULL) {
      /* previous hop can't be next hop */
      if (this == n) continue;

      /* next hop can't be cheaper than previous hop (because then we'd already
       * have tried it) */
      if (this->cost < n->cost) continue;

      /* nor can it be equally costly and before */
      if (this->cost == n->cost && i < previ) continue;
    }

    /* any node that get's here is valid, so pick the best */
    /* if we don't have a best, then this is the best */
    if (next == NULL) {
      next = this;
      nexti = i;
      continue;
    }

    /* if this is closer, use it */
    if (this->cost < next->cost) {
      next = this;
      nexti = i;
      continue;
    /* if it is further away, don't use it */
    } else if (this->cost > next->cost) continue;

    /* if this is newer, use it */
    if (this->node->last_active < next->node->last_active) {
      next = this;
      nexti = i;
      continue;
    /* if it is older, don't use it */
    } else if (this->node->last_active < next->node->last_active) continue;

    /* if we're closer to the front of the list, use it */
    if (i < nexti) {
      next = this;
      nexti = i;
      continue;
    }
  }

  if (next == n || next == NULL) {
    /* no next route found */
    PRINTF("subnet: no next hop =(\n");
    return NULL;
  }

  PRINTF("subnet: next hop to try is %d.%d\n", next->node->addr.u8[0], next->node->addr.u8[1]);

  return &next->node->addr;
}

static void broadcast(struct disclose_conn *c) {
  disclose_send(c, &rimeaddr_null);
}

/* because REVOKED subscriptions are still known */
static bool is_known(struct subnet_conn *c, short sinkid, subid_t subid) {
  enum existance e = c->u->exists(c, sinkid, subid);
  return e == UNKNOWN ? false : true;
}

static void notify_left(struct subnet_conn *c, const rimeaddr_t *sink) {
  /* this sink has been revoked, let neighbours know! */
  prepare_packetbuf(SUBNET_PACKET_TYPE_LEAVING, sink, 0);
  broadcast(&c->pubsub);
}

static void handle_leaving(struct subnet_conn *c, const rimeaddr_t *sink) {
  short sinkid = find_sinkid(c, sink);
  struct sink *s;

  if (sinkid == -1) return;

  s = &c->sinks[sinkid];
  if (s->revoked != 0) return;

  c->u->sink_left(c, sinkid);
  s->revoked = clock_seconds();
  s->numhops = 0;
  s->advertised_cost = 0;
  notify_left(c, sink);
}

static void update_routes(struct subnet_conn *c, const rimeaddr_t *sink, const rimeaddr_t *from) {
  int i;
  short replacesinkid = -1;
  struct neighbor *n = NULL;
  struct sink *route = NULL;
  struct neighbor *oldest;
  struct sink_neighbor *replace;
  short cost = packetbuf_attr(PACKETBUF_ATTR_HOPS);
  int replacei = 0;

  PRINTF("subnet: updating routing table\n");

  /* find route to sink */
  for (i = 0; i < c->numsinks; i++) {
    if (rimeaddr_cmp(&c->sinks[i].sink, sink)) {
      route = &c->sinks[i];
      PRINTF("subnet: found sink @ %d\n", i);
    }
    if (replacesinkid == -1 &&
        c->sinks[i].revoked > 0 &&
        clock_seconds() - c->sinks[i].revoked > SUBNET_REVOKE_PERIOD) {
      PRINTF("subnet: found revoked sink %d\n", i);
      replacesinkid = i;
    }
  }

  /* create sink node if not found */
  if (route == NULL) {
    if (replacesinkid == -1 && c->numsinks >= SUBNET_MAX_SINKS) {
      PRINTF("subnet: max sinks limit hit\n");
      return;
    } else {
      if (c->numsinks < SUBNET_MAX_SINKS) {
        PRINTF("subnet: new sink node %d created for %d.%d\n", c->numsinks, sink->u8[0], sink->u8[1]);
        route = &c->sinks[c->numsinks];
        c->numsinks++;
      } else {
        PRINTF("subnet: old sink node %d replaced with %d.%d\n", replacesinkid, sink->u8[0], sink->u8[1]);
        route = &c->sinks[replacesinkid];
      }

      memset(route, 0, sizeof(struct sink));
      rimeaddr_copy(&route->sink, sink);
      if (rimeaddr_cmp(from, &rimeaddr_null)) {
        route->advertised_cost = 0;
      } else {
        route->advertised_cost = cost + 1;
      }
      PRINTF("subnet: advertised cost will be %d\n", route->advertised_cost);
    }
  }

  /* if we didn't hear this subscription from someone else, we're done */
  if (rimeaddr_cmp(from, &rimeaddr_null)) {
    PRINTF("subnet: we sent the packet, so no need to update neighbors\n");
    return;
  }

  PRINTF("subnet: update neighbor %d.%d\n", from->u8[0], from->u8[1]);

  /* find neighbor pointer */
  oldest = n;
  for (i = 0; i < c->numneighbors; i++) {
    if (rimeaddr_cmp(&c->neighbors[i].addr, from)) {
      n = &c->neighbors[i];
    }

    if (oldest == NULL || c->neighbors[i].last_active < oldest->last_active) {
      oldest = &c->neighbors[i];
    }
  }

  /* create neighbor if not found */
  if (n == NULL) {
    if (c->numneighbors >= SUBNET_MAX_NEIGHBORS) {
      PRINTF("subnet: max neighbours limit hit\n");
      n = oldest;
    } else {
      PRINTF("subnet: new neighbour node created for %d.%d\n", from->u8[0], from->u8[1]);
      n = &c->neighbors[c->numneighbors];
      c->numneighbors++;
    }

    rimeaddr_copy(&n->addr, from);
  }

  n->last_active = clock_seconds();

  /* find cheapest and oldest next hop towards sink */
  for (i = 0; i < route->numhops; i++) {
    if (rimeaddr_cmp(&route->nexthops[i].node->addr, from)) {
      route->nexthops[i].cost = cost;
      n = NULL;
    }

    if (route->nexthops[i].node == oldest) {
      replacei = i;
    }
  }

  /* if from was not stored as a next hop for sink and it is indeed a *next*
   * hop, add it (or replace with oldest).
   * Need strictly less-than here because we haven't added 1 to the cost of the
   * incoming packet. */
  if (n != NULL && cost < route->advertised_cost) {
    PRINTF("subnet: viable next hop for %d.%d found: %d.%d (%d <= %d)\n"
        , sink->u8[0]
        , sink->u8[1]
        , from->u8[0]
        , from->u8[1]
        , cost
        , route->advertised_cost);
    if (route->numhops < SUBNET_MAX_ALTERNATE_ROUTES) {
      replace = &route->nexthops[route->numhops];
      route->numhops++;
    /* } else if (oldest != n) { no need for this check since n is always new */
    } else {
      replace = &route->nexthops[replacei];
    }

    replace->node = n;
    replace->cost = cost;
  }
}

static void handle_subscriptions(struct subnet_conn *c, const rimeaddr_t *sink, const rimeaddr_t *from) {
  bool subscribe = (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_SUBSCRIBE);
  bool broadcasted = false;
  short sinkid;

  if (c->u->exists == NULL) {
    return;
  }

  update_routes(c, sink, from);
  sinkid = find_sinkid(c, sink);

  EACH_PACKET_FRAGMENT(
    if (!is_known(c, sinkid, subid) == subscribe) {
      if (!broadcasted) {
        PRINTF("subnet: new subscription (%d) in packet, forwarding...\n", subid);
        /* something changed, send new subscription to neighbours */
        packetbuf_set_attr(PACKETBUF_ATTR_HOPS, packetbuf_attr(PACKETBUF_ATTR_HOPS)+1);
        broadcast(&c->pubsub);
        broadcasted = true;
      }

      if (subscribe) {
        PRINTF("subnet: packet contained new subscription %d\n", subid);
        c->u->subscribe(c, sinkid, subid, payload);
      } else {
        PRINTF("subnet: packet contained unsubscription for %d\n", subid);
        c->u->unsubscribe(c, sinkid, subid);
      }
    }
  );
}
/*---------------------------------------------------------------------------*/
/* private callback function definitions */
static void on_peer(struct disclose_conn *disclose, const rimeaddr_t *from) {
  /* peer points to second disclose_conn, so we need to decrement to cast to
   * subnet_conn */
  struct subnet_conn *c = (struct subnet_conn *)(disclose-1);
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);

  if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_ASK) {
    PRINTF("subnet: heard peer ask packet from %d.%d\n", from->u8[0], from->u8[1]);
    if (c->u->inform == NULL) {
      return;
    }

    {
      short sinkid = find_sinkid(c, sink);
      struct sink *s;
      struct peer_packet *p = packetbuf_dataptr();
      subid_t *revoked = (subid_t *)(p+1);
      subid_t unknown[p->unknown];
      short fragments = 0;
      struct fragment *frag;
      int i;

      if (sinkid == -1) return;
      s = &c->sinks[sinkid];

      if (s->revoked == 0) {
        /* notify upstream about revoked subs */
        for (i = 0; i < p->revoked; i++) {
          if (c->u->exists(c, sinkid, *revoked) == KNOWN) {
            c->u->unsubscribe(c, sinkid, *revoked);
          }
          revoked++;
        }
      }

      if (!rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &rimeaddr_node_addr)) {
        /* don't reply if we're not being asked */
        return;
      }

      if (s->revoked != 0) {
        /* sink has left, let asker know! */
        notify_left(c, sink);
        return;
      }

      /* read in values for unknown so we can reuse the packetbuf */
      for (i = 0; i < p->unknown; i++) {
        unknown[i] = *(revoked+i);
      }

      prepare_packetbuf(SUBNET_PACKET_TYPE_REPLY, sink, s->advertised_cost);

      frag = packetbuf_dataptr();
      dlen_t sz = 0;
      for (i = 0; i < p->unknown; i++) {
        /* set subid and write data to frag+1 (straight after header) */
        frag->subid = unknown[i];
        frag->length = c->u->inform(c, sinkid, unknown[i], frag+1, PACKETBUF_SIZE-sz-sizeof(struct fragment));

        if (frag->length == 0) {
          /* don't put an empty fragment in there */
          continue;
        }

        /* update total length and number of included fragments */
        sz += frag->length;
        fragments++;

        /* write move pointer past written data */
        SKIPBYTES(frag, struct fragment *, frag->length + sizeof(struct fragment));

      }

      packetbuf_set_datalen(sz);
      packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, fragments);
    }

    /* packetbuf now holds info about subscription */
    disclose_send(&c->peer, from);

  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_REPLY) {
    PRINTF("subnet: heard peer reply packet from %d.%d\n", from->u8[0], from->u8[1]);
    handle_subscriptions(c, sink, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_LEAVING) {
    PRINTF("subnet: heard peer leaving packet from %d.%d\n", from->u8[0], from->u8[1]);
    handle_leaving(c, sink);
  }
}

/**
 * This is called when a downstream node sends a publish message to us as next
 * hop to forward towards the sink, so forward. Subscription must be known to us
 * here since otherwise we wouldn't be next hop.
 */
static void on_recv(struct disclose_conn *disclose, const rimeaddr_t *from) {
  struct subnet_conn *c = (struct subnet_conn *)disclose;
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);
  short sinkid;
  struct sink *s;
  char buf[PACKETBUF_SIZE];

  PRINTF("subnet: got publish packet from downstream node %d.%d\n", from->u8[0], from->u8[1]);
  if (c->u->ondata == NULL) {
    return;
  }

  sinkid = find_sinkid(c, sink);
  if (sinkid == -1) return;

  s = &c->sinks[sinkid];
  if (s->revoked != 0) {
    notify_left(c, sink);
    return;
  }

  PRINTF("subnet: incoming packet has %d fragments\n", packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS));

  // use a buffer to allow ondata to use the packetbuf (e.g. decide to send)
  memcpy(buf, packetbuf_dataptr(), packetbuf_datalen());
  EACH_FRAGMENT(
    packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS),
    buf,
    if (frag->length > 0) {
      PRINTF("        fragment %d is %d bytes for %d...\n", fragi, frag->length, subid);
      c->u->ondata(c, sinkid, subid, payload);
    } else {
      PRINTF("        fragment %d is empty - ignoring\n", fragi);
    }
  );
}

/**
 * Called when a neighbouring node sends a publish that is not destined for us,
 * or if we hear a subscribe packet from another node.
 * If it is a publish and the subscription is unknown, ask peer for it.
 * If it is a subscribe and the subscription is unknown, add subscription.
 */
static void on_hear(struct disclose_conn *disclose, const rimeaddr_t *from) {
  struct subnet_conn *c = (struct subnet_conn *)disclose;
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);

  if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_SUBSCRIBE) {
    PRINTF("subnet: heard subscribe packet from %d.%d\n", from->u8[0], from->u8[1]);
    handle_subscriptions(c, sink, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_LEAVING) {
    PRINTF("subnet: heard leaving packet from %d.%d\n", from->u8[0], from->u8[1]);
    handle_leaving(c, sink);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_UNSUBSCRIBE) {
    PRINTF("subnet: heard unsubscribe packet from %d.%d\n", from->u8[0], from->u8[1]);
    handle_subscriptions(c, sink, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_PUBLISH) {

    PRINTF("subnet: heard publish packet from %d.%d\n", from->u8[0], from->u8[1]);
    if (c->u->exists == NULL) {
      PRINTF("subnet: no exists function in callback, ignoring...\n");
      return;
    }

    {
      /* ask peer for clarification */
      short fragments = packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS);
      struct peer_packet p = { 0, 0 };
      dlen_t written = sizeof(struct peer_packet);

      subid_t revoked[fragments];
      subid_t unknown[fragments];

      short sinkid = find_sinkid(c, sink);
      if (sinkid != -1) {
        if (c->sinks[sinkid].revoked != 0) {
          PRINTF("subnet: sink in publish packet has left, notifying...\n");
          notify_left(c, sink);
          return;
        }
      }

      EACH_PACKET_FRAGMENT(
        if (sinkid == -1) {
          unknown[p.unknown++] = subid;
          continue;
        }

        switch (c->u->exists(c, sinkid, subid)) {
        case REVOKED:
          revoked[p.revoked++] = subid;
          break;
        case UNKNOWN:
          unknown[p.unknown++] = subid;
          break;
        case KNOWN:
          break;
        }
      );

      PRINTF("subnet: packet contains %d unknown and %d revoked subscriptions\n",
          p.unknown,
          p.revoked);

      if (p.unknown == 0 && p.revoked == 0) {
        return;
      }

      /* base attrs and length struct */
      prepare_packetbuf(SUBNET_PACKET_TYPE_ASK, sink, 0);
      memcpy(packetbuf_dataptr(), &p, sizeof(struct peer_packet));

      /* write revoked */
      memcpy(packetbuf_dataptr() + written, revoked, p.revoked * sizeof(subid_t));
      written += p.revoked * sizeof(subid_t);

      /* write unknown */
      memcpy(packetbuf_dataptr() + written, unknown, p.unknown * sizeof(subid_t));
      packetbuf_set_datalen(written + p.unknown * sizeof(subid_t));

      /* send and restore */
      /* TODO: Avoid congestion if all neighbours also send ask */
      disclose_send(&c->peer, from);
    }
  }
}

/**
 * Called if a forwarding/sending succeeded. Should result in an upstream
 * callback
 */
static void on_sent(struct disclose_conn *disclose, int status) {
  struct subnet_conn *c = (struct subnet_conn *)disclose;
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);
  const rimeaddr_t *prevto = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);
  const rimeaddr_t *nexthop;
  short sinkid = find_sinkid(c, sink);
  struct sink *s = &c->sinks[sinkid];

  if (!rimeaddr_cmp(prevto, &rimeaddr_null)) {
    if (status != MAC_TX_OK) {
      nexthop = get_next_hop(c, s, prevto);
      PRINTF("subnet: send to %d.%d via %d.%d failed\n",
          sink->u8[0], sink->u8[1],
          prevto->u8[0], prevto->u8[1]);

      if (nexthop == NULL) {
        PRINTF("subnet: no next hop to try, adding %d fragments back\n", queuebuf_attr(c->sentpacket, PACKETBUF_ATTR_EFRAGMENTS));

        {
          char buf[PACKETBUF_SIZE];
          // use a buffer to allow ondata to use the packetbuf (e.g. decide to send)
          memcpy(buf, queuebuf_dataptr(c->sentpacket), queuebuf_datalen(c->sentpacket));
          PRINTF("subnet: sent packet was %d bytes\n", queuebuf_datalen(c->sentpacket));

          EACH_FRAGMENT(
            queuebuf_attr(c->sentpacket, PACKETBUF_ATTR_EFRAGMENTS),
            buf,
            if (frag->length > 0) {
              PRINTF("        fragment %d is %d bytes for %d...\n", fragi, frag->length, subid);
              c->u->ondata(c, sinkid, subid, payload);
            } else {
              PRINTF("        fragment %d is empty - ignoring\n", fragi);
            }
          );
        }

        if (c->sentpacket != NULL) {
          queuebuf_free(c->sentpacket);
          c->sentpacket = NULL;
        }

        if (c->u->errpub != NULL) {
          c->u->errpub(c);
        }

        return;
      }

      PRINTF("subnet: trying %d.%d instead\n",
          nexthop->u8[0], nexthop->u8[1]);

      queuebuf_to_packetbuf(c->sentpacket);
      disclose_send(&c->pubsub, nexthop);
      return;
    }

    PRINTF("subnet: packet sent\n");

    if (c->sentpacket != NULL) {
      queuebuf_free(c->sentpacket);
      c->sentpacket = NULL;
    }
  } else {
    if (status == MAC_TX_OK) {
      PRINTF("subnet: broadcast packet sent\n");
    } else {
      PRINTF("subnet: broadcast packet failed to send, but doesn't matter\n");
    }
  }
}

static void prepare_packetbuf(uint8_t type, const rimeaddr_t *sink, uint8_t hops) {
  PRINTF("subnet: preparing packet for %d.%d\n", sink->u8[0], sink->u8[1]);

  packetbuf_clear();
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, type);
  packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, 0);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, sink);
  packetbuf_set_attr(PACKETBUF_ATTR_HOPS, hops);
}

static bool inject_packetbuf(subid_t subid, dlen_t bytes, uint8_t *fragments, dlen_t *buflen, void *payload, void *buf) {
  struct fragment *f = buf;
  dlen_t sz = sizeof(struct fragment) + bytes;
  dlen_t blen;
  uint8_t frags;

  PRINTF("subnet: writing %d bytes (%d data) for subid %d\n", sz, bytes, subid);

  if (buflen == NULL) {
    blen = packetbuf_datalen() + sz;
  } else {
    blen = *buflen + sz;
  }

  if (blen > PACKETBUF_SIZE) {
    return false;
  }

  if (fragments == NULL) {
    frags = packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS) + 1;
  } else {
    frags = *fragments + 1;
  }

  if (f == NULL) {
    f = packetbuf_dataptr();
  }

  f->subid = subid;
  f->length = bytes;

  if (bytes > 0 && payload != NULL) {
    memcpy(f+1, payload, bytes);
  }

  if (fragments == NULL) {
    packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, frags);
  } else {
    *fragments = frags;
  }

  if (buflen == NULL) {
    packetbuf_set_datalen(blen);
  } else {
    *buflen = blen;
  }

  PRINTF("subnet: write successful, total now %d in %d fragments\n", blen, frags);
  return true;
}
/*---------------------------------------------------------------------------*/
/** @} */
