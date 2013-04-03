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
/* TODO: update last active for "from" for all recv/hear callbacks */
/*---------------------------------------------------------------------------*/
static const rimeaddr_t* get_next_hop(struct subnet_conn *c, const struct subscription *s) {
}
static void broadcast(struct adisclose_conn *c) {
  /* note that we can use disclose here since we don't need any callbacks,
   * adisclose has no disclose callback on send and adisclose_conn has
   * disclose_conn as its first member. We use disclose rather than adisclose
   * since this is a broadcast and we don't want to wait for ACKs */
  disclose_send(c, &rimeaddr_node_addr);
}
/*---------------------------------------------------------------------------*/
static void on_peer_recv(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno) {
  /* peer points to second adisclose_conn, so we need to decrement to cast to
   * subnet_conn */
  adisclose--;
  struct subnet_conn *c = (struct adisclose_conn *)adisclose;
  struct subscription s = { packetbuf_addr(PACKETBUF_ADDR_ERECEIVER),
                            packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID) };

  if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_ASK) {
    if (c->u->inform == NULL) {
      return;
    }

    c->u->inform(c, &s);
    /* packetbuf now holds info about subscription */

    packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_REPLY);
    adisclose_send(&c->peer, from);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_REPLY) {
    if (c->u->exists == NULL || c->u->exists(c, &s)) {
      return;
    }

    /* send new subscription to neighbours */
    broadcast(&c->pubsub);

    if (c->u->subscribe != NULL) {
      c->u->subscribe(c, &s);
    }
  }
}

static void on_peer_hear(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno) {
  /* peer points to second adisclose_conn, so we need to decrement to cast to
   * subnet_conn */
  adisclose--;
  struct subnet_conn *c = (struct adisclose_conn *)adisclose;
  struct subscription s = { packetbuf_addr(PACKETBUF_ADDR_ERECEIVER),
                            packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID) };

  if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_REPLY) {
    if (c->u->exists == NULL || c->u->exists(c, &s)) {
      return;
    }

    /* send new subscription to neighbours */
    broadcast(&c->pubsub);

    if (c->u->subscribe != NULL) {
      c->u->subscribe(c, &s);
    }
  }
}
/*---------------------------------------------------------------------------*/
/**
 * This is called when a downstream node sends a publish message to us as next
 * hop to forward towards the sink, so forward. Subscription must be known to us
 * here since otherwise we wouldn't be next hop.
 */
static void on_recv(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno) {
  struct subnet_conn *c = (struct adisclose_conn *)adisclose;

  PRINTF("%d.%d: subnet: recv_from_adisclose from %d.%d type %d seqno %d\n",
         rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
         from->u8[0], from->u8[1],
         packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE),
         packetbuf_attr(PACKETBUF_ATTR_PACKET_ID));

  struct subscription s = { packetbuf_addr(PACKETBUF_ADDR_ERECEIVER),
                            packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID) };

  PRINTF("%d.%d: subnet: got PUBLISH for %d.%d from %d.%d, subid %d\n",
     rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
     s->sink->u8[0], s->sink->u8[1],
     from->u8[0], from->u8[1],
     s->subid);

  if (c->u->ondata != NULL) {
    c->u->ondata(c, &s);
  }
}

/**
 * Called when a neighbouring node sends a publish that is not destined for us,
 * or if we hear a subscribe packet from another node.
 * If it is a publish and the subscription is unknown, ask peer for it.
 * If it is a subscribe and the subscription is unknown, add subscription.
 */
static void on_hear(struct adisclose_conn *adisclose, const rimeaddr_t *from, uint8_t seqno) {
  struct subnet_conn *c = (struct adisclose_conn *)adisclose;

  PRINTF("%d.%d: subnet: hear_from_adisclose from %d.%d type %d seqno %d\n",
         rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
         from->u8[0], from->u8[1],
         packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE),
         packetbuf_attr(PACKETBUF_ATTR_PACKET_ID));

  struct subscription s = { packetbuf_addr(PACKETBUF_ADDR_ERECEIVER),
                            packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID) };

  if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_SUBSCRIBE) {

    PRINTF("%d.%d: subnet: heard SUBSCRIBE from %d.%d via %d.%d, subid %d\n",
       rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
       s->sink->u8[0], s->sink->u8[1],
       from->u8[0], from->u8[1],
       s->subid);

    if (c->u->exists == NULL || c->u->exists(c, &s)) {
      return;
    }

    /* send new subscription to neighbours */
    broadcast(&c->pubsub);

    if (c->u->subscribe != NULL) {
      c->u->subscribe(c, &s);
    }

  } else (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_UNSUBSCRIBE) {

    PRINTF("%d.%d: subnet: heard UNSUBSCRIBE from %d.%d via %d.%d, subid %d\n",
       rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
       s->sink->u8[0], s->sink->u8[1],
       from->u8[0], from->u8[1],
       s->subid);

    if (c->u->exists == NULL || !c->u->exists(c, &s)) {
      return;
    }

    /* send new subscription to neighbours */
    broadcast(&c->pubsub);

    if (c->u->unsubscribe != NULL) {
      c->u->unsubscribe(c, &s);
    }

  } else (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_PUBLISH) {

    PRINTF("%d.%d: subnet: heard PUBLISH for %d.%d from %d.%d, subid %d\n",
       rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
       s->sink->u8[0], s->sink->u8[1],
       from->u8[0], from->u8[1],
       s->subid);

    if (c->u->exists == NULL || c->u->exists(c, &s)) {
      return;
    }

    /* ask peer for clarification */
    packetbuf_clear();
    packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_ASK);
    packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, s->subid);
    packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, s->sink);
    packetbuf_copyto(&s);
    adisclose_send(&c->peer, from);
  }
}

/**
 * Called if a forwarding/sending failed. Should try next host in alternate list
 * or call errpub callback if no more alternates are available
 */
static void on_timedout(struct adisclose_conn *c, const rimeaddr_t *to) {
  /* TODO */
}

/**
 * Called if a forwarding/sending succeeded. Should result in an upstream
 * callback
 */
static void on_sent(struct adisclose_conn *adisclose, const rimeaddr_t *to) {
  struct subnet_conn *c = (struct adisclose_conn *)adisclose;
  struct subscription s = { packetbuf_attr(PACKETBUF_ADDR_ERECEIVER),
                            packetbuf_attr(PACKETBUF_ATTR_EPACKET_ID) };
  if (c->u->onsent != NULL) {
    c->u->onsent(c, &s);
  }
}
/*---------------------------------------------------------------------------*/
static const struct adisclose_callbacks subnet = {
  on_recv,
  on_sent,
  on_timedout,
  on_hear,
};
static const struct adisclose_callbacks peer = {
  on_peer_recv,
  NULL,
  NULL,
  on_peer_hear,
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

void subnet_publish(struct subnet_conn *c, const struct subscription *s) {
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_PUBLISH);
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, s->subid);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, s->sink);
  rimeaddr_t nexthop = get_next_hop(c, s);
  adisclose_send(&c->pubsub, nexthop);
}

short subnet_subscribe(struct subnet_conn *c) {
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_SUBSCRIBE);
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, ++c->subid);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, &rimeaddr_node_addr);
  broadcast(&c->pubsub);
  return c->subid;

  /* TODO: has to be send periodically */
}

void subnet_unsubscribe(struct subnet_conn *c, short subid) {
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_UNSUBSCRIBE);
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, subid);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, &rimeaddr_node_addr);
  broadcast(&c->pubsub);

  /* TODO: has to be send periodically and cancel periodic sending of
   * corresponding subscribe */
}
/** @} */
