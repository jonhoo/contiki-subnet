/**
 * \addtogroup rime
 * @{
 */
/**
 * \defgroup subnet Subscription-oriented network protocol
 * @{
 *
 * The subnet networking protocol maintains subscriptions and sink routing
 * information
 */

/**
 * \file   Header file for the Subnet networking protocol
 * \author Jon Gjengset <jon@tsp.io>
 */

#ifndef __SUBNET_H__
#define __SUBNET_H__

#include "net/rime/disclose.h"
#include "net/rime/rimeaddr.h"
#include <stdbool.h>

#ifdef SUBNET_CONF_MAX_SINKS
#define SUBNET_MAX_SINKS SUBNET_CONF_MAX_SINKS
#else
#define SUBNET_MAX_SINKS 5
#endif

#ifdef SUBNET_CONF_MAX_NEIGHBORS
#define SUBNET_MAX_NEIGHBORS SUBNET_CONF_MAX_NEIGHBORS
#else
#define SUBNET_MAX_NEIGHBORS 10
#endif

#ifdef SUBNET_CONF_MAX_ALTERNATE_ROUTES
#define SUBNET_MAX_ALTERNATE_ROUTES SUBNET_CONF_MAX_ALTERNATE_ROUTES
#else
#define SUBNET_MAX_ALTERNATE_ROUTES 3
#endif

#define SUBNET_PACKET_TYPE_SUBSCRIBE 0
#define SUBNET_PACKET_TYPE_REPLY 0
#define SUBNET_PACKET_TYPE_PUBLISH 1
#define SUBNET_PACKET_TYPE_ASK 1
#define SUBNET_PACKET_TYPE_UNSUBSCRIBE 2
#define SUBNET_PACKET_TYPE_INVALIDATE SUBNET_PACKET_TYPE_UNSUBSCRIBE
#define SUBNET_PACKET_TYPE_LEAVING 3

#define SUBNET_ATTRIBUTES  { PACKETBUF_ATTR_EPACKET_TYPE, 2*PACKETBUF_ATTR_BIT }, \
                           { PACKETBUF_ATTR_EFRAGMENTS,   8*PACKETBUF_ATTR_BIT }, \
                           { PACKETBUF_ATTR_HOPS,         4*PACKETBUF_ATTR_BIT }, \
                           { PACKETBUF_ADDR_ERECEIVER,      PACKETBUF_ADDRSIZE }, \
                             DISCLOSE_ATTRIBUTES

#ifdef SUBNET_CONF_REVOKE_PERIOD
#define SUBNET_REVOKE_PERIOD SUBNET_CONF_REVOKE_PERIOD
#else
#define SUBNET_REVOKE_PERIOD 600
#endif


typedef uint8_t subid_t;
typedef uint8_t dlen_t;
/*---------------------------------------------------------------------------*/
/* private structs */
/**
 * \brief Header for peer packets
 */
struct peer_packet {
  short revoked;
  short unknown;
};

/**
 * \brief Header for each subscription in pubsub packets
 */
struct fragment {
  subid_t subid;
  dlen_t length;
};

/**
 * \brief Information about a single neighbor
 */
struct neighbor {
  rimeaddr_t addr;
  clock_time_t last_active; /* last time this next hop was heard from */
};

/**
 * \brief Information about a single next hop
 */
struct sink_neighbor {
  uint8_t cost; /* advertised cost for this hop by next hop */
  struct neighbor *node;
};

/**
 * \brief Information about next hops to a single sink
 */
struct sink {
  rimeaddr_t sink;
  uint8_t advertised_cost; /* what cost we've advertised this route as */
  uint8_t numhops;
  struct sink_neighbor nexthops[SUBNET_MAX_ALTERNATE_ROUTES];

  uint8_t fragments;
  dlen_t buflen;
  char buf[PACKETBUF_SIZE];

  clock_time_t revoked;
};
/*---------------------------------------------------------------------------*/
/* public structs */
/**
 * \brief Subnet connection state
 */
struct subnet_conn {
  struct disclose_conn pubsub;     /* connection for pub/sub messages */
  struct disclose_conn peer;       /* connection for P2P subscription info */
  const struct subnet_callbacks *u; /* callbacks */
  subid_t subid;                        /* last sent subscription id */

  uint8_t numsinks;                   /* number of routes (i.e. sinks) known */
  struct sink sinks[SUBNET_MAX_SINKS];

  uint8_t numneighbors;               /* number of neighbors known */
  struct neighbor neighbors[SUBNET_MAX_NEIGHBORS];

  struct queuebuf *sentpacket;      /* store a publish message until it has been
                                       sent to the next hop */

  short writeout;
  struct sink writesink;
};

enum existance {
  UNKNOWN,
  KNOWN,
  REVOKED
};

/* implementors can expect exists to have returned the appropriate value before
 * calls to subscribe, unsubscribe and inform */
struct subnet_callbacks {
  /* called if no next hop can be contacted */
  void (* errpub)(struct subnet_conn *c);

  /* called when a publish is received. Note that data MUST be copied if it is
   * to be reused later as the memory WILL be reclaimed. */
  void (* ondata)(struct subnet_conn *c, short sinkid, subid_t subid, void *data);

  /* called when a new subscription is in packetbuf. Note that data MUST be
   * copied if it is to be reused later as the memory WILL be reclaimed */
  void (* subscribe)(struct subnet_conn *c, short sinkid, subid_t subid, void *data);

  /* called when a subscription is cancelled */
  void (* unsubscribe)(struct subnet_conn *c, short sinkid, subid_t subid);

  /* should return true if the given subscription is known. Note that this
   * function may be called quite frequently, so it may be well worth to
   * optimize */
  enum existance (* exists)(struct subnet_conn *c, short sinkid, subid_t subid);

  /* should fill target with information about the given subscription and return
   * the number of bytes written. It is up to this function to make sure it
   * does not write more than the given amount of bytes. If the function
   * determines it cannot do with so few bytes, it should return 0 */
  dlen_t (* inform)(struct subnet_conn *c, short sinkid, subid_t subid, void *target, dlen_t space);

  /* called when a sink has indicated that it is leaving for good. Should revoke
   * all subscriptions to this sink. Note that this sinkid may be reused in the
   * future! */
  void (* sink_left)(struct subnet_conn *c, short sinkid);
};
/*---------------------------------------------------------------------------*/
/* public functions */
/**
 * \brief Open a new subscription connection
 * \param c Memory space for connection state
 * \param subchannel Channel on which to communicate pub/sub messages
 * \param peerchannel Channel for P2P communication for subscription info
 * \param u User callbacks
 */
void subnet_open(struct subnet_conn *c,
                 uint16_t subchannel,
                 uint16_t peerchannel,
                 const struct subnet_callbacks *u);

/**
 * \brief End all subscriptions and close subnet connection
 * \param c Connection state
 *
 * Note that this function *MUST* be called before a sink quits to free up
 * resources in the network, otherwise the sink's later subscriptions may be
 * ignored!
 */
void subnet_close(struct subnet_conn *c);

/**
 * \brief Add data for a subscription to the current publish
 * \param c Connection state
 * \param sinkid Sink to send data to
 * \param subid Subscription data is being added for
 * \param payload Data
 * \param bytes Number of bytes of data being added
 * \return True if data was added, false if no more data can be added
 */
bool subnet_add_data(struct subnet_conn *c, short sinkid, subid_t subid, void *payload, dlen_t bytes);

/**
 * \brief Send publishe data packet
 * \param c Connection state
 * \param sink Sink to send data to
 */
void subnet_publish(struct subnet_conn *c, short sinkid);

/**
 * \brief Send out a new subscription
 * \param c Connection state
 * \param payload Where to read the subscription data from
 * \param bytes Size of the subscription data
 * \return The subscription id of the new subscription
 */
subid_t subnet_subscribe(struct subnet_conn *c, void *payload, dlen_t bytes);

/**
 * \brief Send out a new subscription
 * \param c Connection state
 * \param subid Subscription id to resubscribe to
 * \param payload Where to read the subscription data from
 * \param bytes Size of the subscription data
 */
void subnet_resubscribe(struct subnet_conn *c, subid_t subid, void *payload, dlen_t bytes);

/**
 * \brief Get current amount of data queued for the given sink
 * \param c Connection state
 * \param sink Sink to check payload size for
 * \return Number of bytes queued, -1 on error
 */
short subnet_packetlen(struct subnet_conn *c, short sinkid);

/**
 * \brief End the given subscription
 * \param c Connection state
 * \param subid Subscription to remove
 */
void subnet_unsubscribe(struct subnet_conn *c, subid_t subid);

/**
 * \brief Returns this node's sink id
 * \param c Connection state
 * \return this node's sink id or -1 if not known
 *
 * Note that this function will only return something sensible after the first
 * subscription has been sent out!
 */
short subnet_myid(struct subnet_conn *c);

/**
 * \brief Redirect all writes to the given sink to a spare buffer
 * \param c Connection state
 * \param sinkid Sink to redirect writes for
 *
 * Note that only a single buffer is available per buffer, so this function can
 * only be active for one sink at the time.
 */
void subnet_writeout(struct subnet_conn *c, short sinkid);

/**
 * \brief Write all data from the spare buffer into the sink's data
 * \param c Connection state
 */
void subnet_writein(struct subnet_conn *c);
/*---------------------------------------------------------------------------*/
/* things that shouldn't *really* be public, but have to be */
/**
 * \brief Extracy payload for given fragment and return a pointer to the next
 * \param frag Current fragment pointer
 * \param payload Pointer to make point to fragment contents
 * \return Pointer to next fragment
 */
struct fragment *next_fragment(struct fragment *frag, void **payload);

/**
 * \brief Get a pointer to the real sink struct for the given sink
 * \param c Connection state
 * \param sinkid Sink to get data for
 * \return The real sink struct for the given sink. Be careful!
 */
const struct sink *subnet_sink(struct subnet_conn *c, short sinkid);

/**
 * \brief Run the given BLOCK for all FRAGMENTS fragments in the given BUF
 * \param FRAGMENTS Expression evaluating to the number of fragments in BUF
 * \param BUF Expression evaluating to a buffer pointer
 * \param BLOCK Code to run for each fragment. CANNOT CONTAIN BREAK/CONTINUE!
 */
#define EACH_FRAGMENT(FRAGMENTS, BUF, BLOCK) \
  { \
    short fragi;                                                 \
    subid_t subid;                                               \
    short fragments = FRAGMENTS;                                 \
    struct fragment *frag = (struct fragment *) BUF;             \
    struct fragment *next = (struct fragment *) BUF;             \
    void *payload;                                               \
                                                                 \
    for (fragi = 0; fragi < fragments; fragi++) {                \
      next = next_fragment(frag, &payload);                      \
      subid = frag->subid;                                       \
      BLOCK                                                      \
      frag = next;                                               \
    } \
  }

/**
 * \brief Run the given BLOCK for each fragment in the given sink's buffer
 * \param S Expression evaluating to a pointer to the sink
 * \param BLOCK Code to run for each fragment. CANNOT CONTAIN BREAK/CONTINUE!
 */
#define EACH_SINK_FRAGMENT(S, BLOCK) \
  EACH_FRAGMENT(S->fragments, S->buf, BLOCK)
/*---------------------------------------------------------------------------*/


#endif /* __SUBNET_H__ */
/** @} */
/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
