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
/* TODO: update last active for "from" for all recv/hear callbacks */
/*---------------------------------------------------------------------------*/
static const rimeaddr_t* get_next_hop(struct subnet_conn *c, const rimeaddr_t *sink) {
  return sink;
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
  /* get char* so we can skip by size_t */
  char *data = (char *) *raw;
  /* move past (length of payload) bytes */
  SKIPBYTES(*raw, struct fragment *, length);
  return subid;
}
/*---------------------------------------------------------------------------*/
void handle_subscriptions(struct subnet_conn *c, const rimeaddr_t *sink) {
  bool unsubscribe = (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_UNSUBSCRIBE);

  if (c->u->exists == NULL) {
    return;
  }

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
    handle_subscriptions(c, sink);
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
    handle_subscriptions(c, sink);
  } else if (packetbuf_attr(PACKETBUF_ATTR_EPACKET_TYPE) == SUBNET_PACKET_TYPE_UNSUBSCRIBE) {
    handle_subscriptions(c, sink);
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
    packetbuf_clear();
    packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_ASK);
    packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, sink);
    packetbuf_copyfrom(&unknowns, numunknowns * sizeof(short));
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
  struct subnet_conn *c = (struct subnet_conn *)adisclose;
  const rimeaddr_t *sink = packetbuf_addr(PACKETBUF_ADDR_ERECEIVER);

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
  packetbuf_clear();
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_PUBLISH);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, sink);
  packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, 0);
  c->fragments = 0;
  c->frag = packetbuf_dataptr();
  packetbuf_set_datalen(0);
}

bool subnet_add_data(struct subnet_conn *c, short subid, void *payload, size_t bytes) {
  uint16_t l = packetbuf_totlen();
  size_t sz = sizeof(struct fragment) + bytes;
  if (l + sz > PACKETBUF_SIZE) {
    return false;
  }

  c->frag->subid = subid;
  c->frag->length = bytes;
  c->frag++;
  c->fragments++;
  memcpy(c->frag, payload, bytes);
  SKIPBYTES(c->frag, struct fragment *, bytes);
  packetbuf_set_datalen(packetbuf_datalen() + sz);
  return true;
}

void subnet_publish(struct subnet_conn *c) {
  packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, c->fragments);
  const rimeaddr_t *nexthop = get_next_hop(c, packetbuf_addr(PACKETBUF_ADDR_ERECEIVER));
  adisclose_send(&c->pubsub, nexthop);
}

short subnet_subscribe(struct subnet_conn *c, void *payload, size_t bytes) {
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_SUBSCRIBE);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, &rimeaddr_node_addr);
  packetbuf_set_attr(PACKETBUF_ATTR_EFRAGMENTS, 1);

  struct fragment *f = packetbuf_dataptr();
  f->subid = ++c->subid;
  f->length = bytes;
  f++;
  memcpy(f, payload, bytes);
  packetbuf_set_datalen(bytes + sizeof(struct fragment));

  broadcast(&c->pubsub);
  return c->subid;
}

void subnet_unsubscribe(struct subnet_conn *c, short subid) {
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_TYPE, SUBNET_PACKET_TYPE_UNSUBSCRIBE);
  packetbuf_set_attr(PACKETBUF_ATTR_EPACKET_ID, subid);
  packetbuf_set_addr(PACKETBUF_ADDR_ERECEIVER, &rimeaddr_node_addr);

  struct fragment *f = packetbuf_dataptr();
  f->subid = subid;
  f->length = 0;
  packetbuf_set_datalen(sizeof(struct fragment));

  broadcast(&c->pubsub);
}
/** @} */
