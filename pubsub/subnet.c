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
#define EACH_PACKET_FRAGMENT(BLOCK) \
  EACH_FRAGMENT(packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS),      \
                packetbuf_dataptr(),                            \
                BLOCK)

#define SKIPBYTES(VAR, TYPE, BYTES) \
  /* note the cast to char* to be able to move in bytes */       \
  VAR = (TYPE)(((char*) VAR)+BYTES);
/*---------------------------------------------------------------------------*/
/* private functions */
static int find_sinkid(struct subnet_conn *c, const rimeaddr_t *sink);
static struct sink *find_sink(struct subnet_conn *c, const rimeaddr_t *sink);
static uint8_t get_advertised_cost(struct subnet_conn *c, const rimeaddr_t *sink);
static const rimeaddr_t* get_next_hop(struct subnet_conn *c, struct sink *route, const rimeaddr_t *prevto);
static void broadcast(struct adisclose_conn *c);
static bool is_known(struct subnet_conn *c, int sinkid, subid_t subid);
static void notify_left(struct subnet_conn *c, const rimeaddr_t *sink);
static void handle_leaving(struct subnet_conn *c, const rimeaddr_t *sink);
static void update_routes(struct subnet_conn *c, const rimeaddr_t *sink, const rimeaddr_t *from);
static void handle_subscriptions(struct subnet_conn *c, const rimeaddr_t *sink, const rimeaddr_t *from);
static void clean_buffers(struct subnet_conn *c, struct sink *s);

static void on_peer(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno);
static void on_recv(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno);
static void on_hear(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno);
static void on_timedout(struct adisclose_conn *adisclose, const rimeaddr_t *to);
static void on_sent(struct adisclose_conn *adisclose, const rimeaddr_t *to);
/*---------------------------------------------------------------------------*/
/* private members */
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
/* public function definitions */
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
  c->writeout = false;
}

void subnet_close(struct subnet_conn *c) {
  packetbuf_clear();
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_LEAVING);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, &rimeaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_HOPS, 0);

  /* TODO: perhaps do this multiple times for good measure? */
  broadcast(&c->pubsub);

  adisclose_close(&c->pubsub);
  adisclose_close(&c->peer);
}

bool subnet_add_data(struct subnet_conn *c, int sinkid, subid_t subid, void *payload, size_t bytes) {
  struct sink *s;

  if (sinkid >= c->numsinks) {
    return false;
  }

  if (c->writeout == sinkid) {
    s = &c->writesink;
  } else {
    s = &c->sinks[sinkid];
  }

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

void subnet_writeout(struct subnet_conn *c, int sinkid) {
  c->writeout = sinkid;
  c->writesink.fragments = 0;
  c->writesink.buflen = 0;
}

void subnet_writein(struct subnet_conn *c) {
  struct sink *s;
  if (c->writeout == -1) return;

  s = &c->sinks[c->writeout];
  s->fragments = c->writesink.fragments;
  s->buflen = c->writesink.buflen;
  memcpy(s->buf, c->writesink.buf, c->writesink.buflen);
  c->writeout = -1;
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

subid_t subnet_subscribe(struct subnet_conn *c, void *payload, size_t bytes) {
  subid_t subid = c->subid;
  subnet_resubscribe(c, subid, payload, bytes);
  c->subid++;
  return subid;
}

void subnet_resubscribe(struct subnet_conn *c, subid_t subid, void *payload, size_t bytes) {
  struct queuebuf *q = queuebuf_new_from_packetbuf();

  packetbuf_clear();
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_SUBSCRIBE);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, &rimeaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, 1);
  packetbuf_set_attr(PACKETBUF_ATTR_HOPS, 0);

  struct fragment *f = packetbuf_dataptr();
  f->subid = subid;
  f->length = bytes;
  f++;
  memcpy(f, payload, bytes);
  packetbuf_set_datalen(sizeof(struct fragment) + bytes);

  if (!is_known(c, subnet_myid(c), subid)) {
    handle_subscriptions(c, &rimeaddr_node_addr, NULL);
    // handle_subscriptions will take care of the broadcast
  } else {
    broadcast(&c->pubsub);
  }

  queuebuf_to_packetbuf(q);
  queuebuf_free(q);
}

void subnet_unsubscribe(struct subnet_conn *c, subid_t subid) {
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

int subnet_myid(struct subnet_conn *c) {
  static int myid = -1;
  if (myid == -1) {
    myid = find_sinkid(c, &rimeaddr_node_addr);
  }
  return myid;
}

subid_t next_fragment(struct fragment **raw, void **payload) {
  subid_t subid = (*raw)->subid;
  size_t length = (*raw)->length;
  /* move past subid + length */
  *raw = *raw + 1;
  /* we're now at the payload */
  *payload = *raw;
  /* move past (length of payload) bytes */
  SKIPBYTES(*raw, struct fragment *, length);
  return subid;
}

const struct sink *subnet_sink(struct subnet_conn *c, int sinkid) {
  return &c->sinks[sinkid];
}
/*---------------------------------------------------------------------------*/
/* private function definitions */
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
  int previ = -1;
  int nexti = -1;
  struct sink_neighbor *n = NULL;
  struct sink_neighbor *next;
  struct sink_neighbor *this;

  if (route == NULL) {
    return NULL;
  }

  if (route->revoked > 0 && clock_seconds() - route->revoked > SUBNET_REVOKE_PERIOD) {
    return NULL;
  }

  /* find neighbor pointer */
  if (prevto != NULL) {
    for (i = 0; i < route->numhops; i++) {
      if (rimeaddr_cmp(&route->nexthops[i].node->addr, prevto)) {
        previ = i;
        n = &route->nexthops[i];
        break;
      }
    }
  }

  /* find next most expensive route after n */
  next = n;
  for (i = 0; i < route->numhops; i++) {
    this = &route->nexthops[i];

    /* previous hop can't be next hop */
    if (this == n) continue;

    if (n != NULL) {
      /* next hop can't be better than previous hop */
      if (this->cost < n->cost) continue;
      /* nor can it be same cost and before */
      if (this->cost == n->cost && i < previ) continue;
    }

    /* any node that get's here is valid, so pick the best */
    /* if we don't have a best, then this is the best */
    if (next == NULL) {
      next = this;
      nexti = i;
      continue;
    }

    /* if this is more expensive, don't use it */
    if (this->cost > next->cost) continue;

    /* if this is same cost and later, don't use it */
    if (this->cost == next->cost && i > nexti) continue;

    /* here, it's either cheaper or earlier, so it's the best */
    next = this;
    nexti = i;
  }

  if (next == n || next == NULL) {
    /* no next route found */
    return NULL;
  }

  return &n->node->addr;
}

static void broadcast(struct adisclose_conn *c) {
  /* note that we can use disclose here since we don't need any callbacks,
   * adisclose has no disclose callback on send and adisclose_conn has
   * disclose_conn as its first member. We use disclose rather than adisclose
   * since this is a broadcast and we don't want to wait for ACKs */
  disclose_send((struct disclose_conn *) c, &rimeaddr_node_addr);
}

/* because REVOKED subscriptions are still known */
static bool is_known(struct subnet_conn *c, int sinkid, subid_t subid) {
  enum existance e = c->u->exists(c, sinkid, subid);
  return e == UNKNOWN ? false : true;
}

static void notify_left(struct subnet_conn *c, const rimeaddr_t *sink) {
  /* this sink has been revoked, let neighbours know! */
  struct queuebuf *q = queuebuf_new_from_packetbuf();
  packetbuf_clear();
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_LEAVING);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, sink);
  broadcast(&c->pubsub);
  queuebuf_to_packetbuf(q);
  queuebuf_free(q);
}

static void handle_leaving(struct subnet_conn *c, const rimeaddr_t *sink) {
  int sinkid = find_sinkid(c, sink);
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
  int replacesinkid = -1;
  struct neighbor *n = NULL;
  struct sink *route = NULL;
  struct neighbor *oldest;
  struct sink_neighbor *replace;
  short cost = packetbuf_attr(PACKETBUF_ATTR_HOPS);
  int replacei;

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
      PRINTF("%d.%d: subnet: max neighbours limit hit\n",
          rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1]);
      n = oldest;
    } else {
      n = &c->neighbors[c->numneighbors];
      c->numneighbors++;
    }

    rimeaddr_copy(&n->addr, from);
  }

  n->last_active = clock_seconds();

  /* find route to sink */
  for (i = 0; i < c->numsinks; i++) {
    if (rimeaddr_cmp(&c->sinks[i].sink, sink)) {
      route = &c->sinks[i];
    }
    if (replacesinkid == -1 &&
        c->sinks[i].revoked > 0 &&
        clock_seconds() - c->sinks[i].revoked > SUBNET_REVOKE_PERIOD) {
      replacesinkid = i;
    }
  }

  /* create sink node if not found */
  if (route == NULL) {
    if (replacesinkid == -1 && c->numsinks >= SUBNET_MAX_SINKS) {
      PRINTF("%d.%d: subnet: max sinks limit hit\n",
          rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1]);
      return;
    } else {
      if (replacesinkid == -1) {
        route = &c->sinks[c->numsinks];
      } else {
        route = &c->sinks[replacesinkid];
      }

      rimeaddr_copy(&route->sink, sink);
      route->advertised_cost = cost + 1;
      route->numhops = 0;
      route->buflen = 0;
      route->fragments = 0;
      route->revoked = 0;

      if (replacesinkid == -1) {
        c->numsinks++;
      }
    }
  }

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

  /* if *from was not stored as a next hop for sink and it is indeed a *next*
   * hop, add it (or replace with oldest) */
  if (n != NULL && cost <= route->advertised_cost) {
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

  if (c->u->exists == NULL) {
    return;
  }

  update_routes(c, sink, from);
  int sinkid = find_sinkid(c, sink);

  EACH_PACKET_FRAGMENT(
    if (!is_known(c, sinkid, subid) == subscribe) {
      /* something changed, send new subscription to neighbours */
      broadcast(&c->pubsub);
      break;
    }
  );

  if (c->u->subscribe == NULL) {
    return;
  }

  EACH_PACKET_FRAGMENT(
    if (!is_known(c, sinkid, subid) == subscribe) {
      if (subscribe) {
        c->u->subscribe(c, sinkid, subid, payload);
      } else {
        c->u->unsubscribe(c, sinkid, subid);
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
/* private callback function definitions */
static void on_peer(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno) {
  /* peer points to second adisclose_conn, so we need to decrement to cast to
   * subnet_conn */
  struct subnet_conn *c = (struct subnet_conn *)(adisclose-1);
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);

  if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_ASK) {
    if (c->u->inform == NULL) {
      return;
    }

    {
      int sinkid = find_sinkid(c, sink);
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

      packetbuf_set_datalen(0);
      frag = packetbuf_dataptr();
      for (i = 0; i < p->unknown; i++) {
        /* increase size by the size of a fragment header */
        packetbuf_set_datalen(packetbuf_datalen() + sizeof(struct fragment));

        /* set subid and write data to frag+1 (straight after header) */
        frag->subid = unknown[i];
        frag->length = c->u->inform(c, sinkid, unknown[i], frag+1);

        if (frag->length == 0) {
          /* don't put an empty fragment in there */
          packetbuf_set_datalen(packetbuf_datalen() - sizeof(struct fragment));
          continue;
        }

        /* update total length and number of included fragments */
        packetbuf_set_datalen(packetbuf_datalen() + frag->length);
        fragments++;

        /* write move pointer past written data */
        SKIPBYTES(frag, struct fragment *, frag->length + sizeof(struct fragment));

      }

      packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_REPLY);
      packetbuf_set_attr(PACKETBUF_ATTR_HOPS, get_advertised_cost(c, sink));
      packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, fragments);
    }

    /* packetbuf now holds info about subscription */
    adisclose_send(&c->peer, from);

  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_REPLY) {
    handle_subscriptions(c, sink, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_LEAVING) {
    handle_leaving(c, sink);
  }
}

/**
 * This is called when a downstream node sends a publish message to us as next
 * hop to forward towards the sink, so forward. Subscription must be known to us
 * here since otherwise we wouldn't be next hop.
 */
static void on_recv(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno) {
  struct subnet_conn *c = (struct subnet_conn *)adisclose;
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);
  int sinkid;
  struct sink *s;

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

  if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_SUBSCRIBE) {
    handle_subscriptions(c, sink, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_LEAVING) {
    handle_leaving(c, sink);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_UNSUBSCRIBE) {
    handle_subscriptions(c, sink, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_PUBLISH) {

    if (c->u->exists == NULL) {
      return;
    }

    {
      /* ask peer for clarification */
      short fragments = packetbuf_attr(PACKETBUF_ATTR_EFRAGMENTS);
      struct queuebuf *q;
      struct peer_packet p = { 0, 0 };

      subid_t revoked[fragments];
      subid_t unknown[fragments];

      int sinkid = find_sinkid(c, sink);
      if (sinkid == -1) return;

      if (c->sinks[sinkid].revoked != 0) {
        notify_left(c, sink);
        return;
      }

      EACH_PACKET_FRAGMENT(
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

      /* store current packetbuf */
      q = queuebuf_new_from_packetbuf();

      /* base attrs and length struct */
      packetbuf_clear();
      packetbuf_copyfrom(&p, sizeof(struct peer_packet));
      packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_ASK);
      packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, sink);

      /* write revoked */
      memcpy(packetbuf_dataptr() + packetbuf_datalen(), revoked, p.revoked * sizeof(subid_t));
      packetbuf_set_datalen(packetbuf_datalen() + p.revoked * sizeof(subid_t));

      /* write unknown */
      memcpy(packetbuf_dataptr() + packetbuf_datalen(), unknown, p.unknown * sizeof(subid_t));
      packetbuf_set_datalen(packetbuf_datalen() + p.unknown * sizeof(subid_t));

      /* send and restore */
      adisclose_send(&c->peer, from);
      queuebuf_to_packetbuf(q);
      queuebuf_free(q);
    }
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
/** @} */