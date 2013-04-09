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

#include "net/rime/adisclose.h"
#include "net/rime/rimeaddr.h"
#include <stdbool.h>

#ifdef SUBNET_CONF_MAX_SINKS
#define SUBNET_MAX_SINKS SUBNET_CONF_MAX_SINKS
#else
#define SUBNET_MAX_SINKS 10
#endif

#ifdef SUBNET_CONF_MAX_NEIGHBORS
#define SUBNET_MAX_NEIGHBORS SUBNET_CONF_MAX_NEIGHBORS
#else
#define SUBNET_MAX_NEIGHBORS 10
#endif

#ifdef SUBNET_CONF_MAX_ALTERNATE_ROUTES
#define SUBNET_MAX_ALTERNATE_ROUTES SUBNET_CONF_MAX_ALTERNATE_ROUTES
#else
#define SUBNET_MAX_ALTERNATE_ROUTES 5
#endif

#define SUBNET_PACKET_TYPE_SUBSCRIBE 0
#define SUBNET_PACKET_TYPE_ASK 0
#define SUBNET_PACKET_TYPE_PUBLISH 1
#define SUBNET_PACKET_TYPE_REPLY 0
#define SUBNET_PACKET_TYPE_UNSUBSCRIBE 2
#define SUBNET_PACKET_TYPE_INVALIDATE SUBNET_PACKET_TYPE_UNSUBSCRIBE
#define SUBNET_PACKET_TYPE_LEAVING 3

#define SUBNET_ATTRIBUTES  { PACKETBUF_ATTR_EPACKET_TYPE, 2*PACKETBUF_ATTR_BIT }, \
                           { PACKETBUF_ATTR_EFRAGMENTS,   8*PACKETBUF_ATTR_BIT }, \
                           { PACKETBUF_ATTR_HOPS,         4*PACKETBUF_ATTR_BIT }, \
                           { PACKETBUF_ADDR_ERECEIVER,      PACKETBUF_ADDRSIZE }, \
                             ADISCLOSE_ATTRIBUTES

#ifdef SUBNET_CONF_REVOKE_PERIOD
#define SUBNET_REVOKE_PERIOD SUBNET_CONF_REVOKE_PERIOD
#else
#define SUBNET_REVOKE_PERIOD 600
#endif


typedef uint8_t subid_t;
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
  size_t length;
};

/**
 * \brief Information about a single neighbor
 */
struct neighbor {
  rimeaddr_t addr;
  unsigned long last_active; /* last time this next hop was heard from */
};

/**
 * \brief Information about a single next hop
 */
struct sink_neighbor {
  short cost; /* advertised cost for this hop by next hop */
  struct neighbor *node;
};

/**
 * \brief Information about next hops to a single sink
 */
struct sink {
  rimeaddr_t sink;
  short advertised_cost; /* what cost we've advertised this route as */
  short numhops;
  struct sink_neighbor nexthops[SUBNET_MAX_ALTERNATE_ROUTES];

  short fragments;
  size_t buflen;
  char buf[PACKETBUF_SIZE];

  clock_time_t revoked;
};
/*---------------------------------------------------------------------------*/
/* public structs */
/**
 * \brief Subnet connection state
 */
struct subnet_conn {
  struct adisclose_conn pubsub;     /* connection for pub/sub messages */
  struct adisclose_conn peer;       /* connection for P2P subscription info */
  const struct subnet_callbacks *u; /* callbacks */
  subid_t subid;                        /* last sent subscription id */

  short numsinks;                   /* number of routes (i.e. sinks) known */
  struct sink sinks[SUBNET_MAX_SINKS];

  short numneighbors;               /* number of neighbors known */
  struct neighbor neighbors[SUBNET_MAX_NEIGHBORS];

  struct queuebuf *sentpacket;      /* store a publish message until it has been
                                       sent to the next hop */
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
  void (* ondata)(struct subnet_conn *c, int sinkid, subid_t subid, void *data);

  /* called when a publish was sent successfully */
  void (* onsent)(struct subnet_conn *c, int sinkid, subid_t subid);

  /* called when a new subscription is in packetbuf. Note that data MUST be
   * copied if it is to be reused later as the memory WILL be reclaimed */
  void (* subscribe)(struct subnet_conn *c, int sinkid, subid_t subid, void *data);

  /* called when a subscription is cancelled */
  void (* unsubscribe)(struct subnet_conn *c, int sinkid, subid_t subid);

  /* should return true if the given subscription is known. Note that this
   * function may be called quite frequently, so it may be well worth to
   * optimize */
  enum existance (* exists)(struct subnet_conn *c, int sinkid, subid_t subid);

  /* should fill target with information about the given subscription and return
   * the number of bytes written. It is up to this function to make sure the
   * packet is not overfilled (by checking packetbuf_totlen()) */
  size_t (* inform)(struct subnet_conn *c, int sinkid, subid_t subid, void *target);

  /* called when a sink has indicated that it is leaving for good. Should revoke
   * all subscriptions to this sink. Note that this sinkid may be reused in the
   * future! */
  void (* sink_left)(struct subnet_conn *c, int sinkid);
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
bool subnet_add_data(struct subnet_conn *c, int sinkid, subid_t subid, void *payload, size_t bytes);

/**
 * \brief Send publishe data packet
 * \param c Connection state
 * \param sink Sink to send data to
 */
void subnet_publish(struct subnet_conn *c, int sinkid);

/**
 * \brief Send out a new subscription
 * \param c Connection state
 * \param payload Where to read the subscription data from
 * \param bytes Size of the subscription data
 * \return The subscription id of the new subscription
 */
subid_t subnet_subscribe(struct subnet_conn *c, void *payload, size_t bytes);

/**
 * \brief Send out a new subscription
 * \param c Connection state
 * \param subid Subscription id to resubscribe to
 * \param payload Where to read the subscription data from
 * \param bytes Size of the subscription data
 */
void subnet_resubscribe(struct subnet_conn *c, subid_t subid, void *payload, size_t bytes);

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
int subnet_myid(struct subnet_conn *c);
/*---------------------------------------------------------------------------*/

#endif /* __SUBNET_H__ */
/** @} */
/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
