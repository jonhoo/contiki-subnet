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

#define SUBNET_ATTRIBUTES  { PACKETBUF_ATTR_EPACKET_TYPE, 2*PACKETBUF_ATTR_BIT }, \
                           { PACKETBUF_ATTR_EFRAGMENTS,   8*PACKETBUF_ATTR_BIT }, \
                           { PACKETBUF_ATTR_HOPS,         4*PACKETBUF_ATTR_BIT }, \
                           { PACKETBUF_ADDR_ERECEIVER,      PACKETBUF_ADDRSIZE }, \
                             ADISCLOSE_ATTRIBUTES
/*---------------------------------------------------------------------------*/
struct fragment {
  short subid;
  size_t length;
};
/*---------------------------------------------------------------------------*/
/**
 * \brief Information about a single next hop
 */
struct neighbor {
  rimeaddr_t addr;
  short cost; /* advertised cost for this hop by next hop */
  unsigned long last_active; /* last time this next hop was heard from */
};

/**
 * \brief Information about next hops to a single sink
 */
struct sink_route {
  rimeaddr_t sink;
  short advertised_cost; /* what cost we've advertised this route as */
  short numhops;
  struct neighbor *nexthops[SUBNET_MAX_ALTERNATE_ROUTES];
};

/**
 * \brief Subnet connection state
 */
struct subnet_conn {
  struct adisclose_conn pubsub;     /* connection for pub/sub messages */
  struct adisclose_conn peer;       /* connection for P2P subscription info */
  const struct subnet_callbacks *u; /* callbacks */
  short subid;                      /* last sent subscription id */

  short fragments;                  /* number of fragments added */
  struct fragment *frag;            /* next fragment pointer */

  short numroutes;                  /* number of routes (i.e. sinks) known */
  struct sink_route routes[SUBNET_MAX_SINKS];

  short numneighbors;               /* number of neighbors known */
  struct neighbor neighbors[SUBNET_MAX_NEIGHBORS];
};
/*---------------------------------------------------------------------------*/
struct subnet_callbacks {
  /* called if no next hop can be contacted */
  void (* errpub)(struct subnet_conn *c);

  /* called when a publish is received. Note that data MUST be copied if it is
   * to be reused later as the memory WILL be reclaimed */
  void (* ondata)(struct subnet_conn *c, const rimeaddr_t *sink, short subid, void *data);

  /* called when a publish was sent successfully */
  void (* onsent)(struct subnet_conn *c, const rimeaddr_t *sink, short subid);

  /* called when a new subscription is in packetbuf. Note that data MUST be
   * copied if it is to be reused later as the memory WILL be reclaimed */
  void (* subscribe)(struct subnet_conn *c, const rimeaddr_t *sink, short subid, void *data);

  /* called when a subscription is cancelled */
  void (* unsubscribe)(struct subnet_conn *c, const rimeaddr_t *sink, short subid);

  /* should return true if the given subscription is known */
  bool (* exists)(struct subnet_conn *c, const rimeaddr_t *sink, short subid);

  /* should fill target with information about the given subscription and return
   * the number of bytes written. It is up to this function to make sure the
   * packet is not overfilled (by checking packetbuf_totlen()) */
  size_t (* inform)(struct subnet_conn *c, const rimeaddr_t *sink, short subid, void *target);
};
/*---------------------------------------------------------------------------*/
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
 * \brief Close subscription connection
 * \param c Connection state
 */
void subnet_close(struct subnet_conn *c);

/**
 * \brief Prepare packetbuf for a publish towards the given sink
 * \param c Connection state
 * \param sink Sink to send data to
 */
void subnet_prepublish(struct subnet_conn *c, const rimeaddr_t *sink);

/**
 * \brief Add data for a subscription to the current publish
 * \param c Connection state
 * \param subid Subscription data is being added for
 * \param payload Data
 * \param bytes Number of bytes of data being added
 * \return True if data was added, false if no more data can be added
 */
bool subnet_add_data(struct subnet_conn *c, short subid, void *payload, size_t bytes);

/**
 * \brief Send publishe data packet
 * \param c Connection state
 */
void subnet_publish(struct subnet_conn *c);

/**
 * \brief Send out a new subscription
 * \param c Connection state
 * \return The subscription id of the new subscription
 */
short subnet_subscribe(struct subnet_conn *c, void *payload, size_t bytes);

/**
 * \brief End the given subscription
 * \param c Connection state
 * \param subid Subscription to remove
 */
void subnet_unsubscribe(struct subnet_conn *c, short subid);

#endif /* __SUBNET_H__ */
/** @} */
/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
