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

#include "net/rime/rimeaddr.h"

#ifdef SUBNET_CONF_SUBSCRIPTION_BITS
#define SUBNET_SUBSCRIPTION_BITS SUBNET_CONF_SUBSCRIPTION_BITS
#else
#define SUBNET_SUBSCRIPTION_BITS 4
#endif

#ifdef SUBNET_CONF_MAX_SINKS
#define SUBNET_MAX_SINKS SUBNET_CONF_MAX_SINKS
#else
#define SUBNET_MAX_SINKS 10
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

#define SUBNET_ATTRIBUTES  { PACKETBUF_ATTR_EPACKET_ID, PACKETBUF_ATTR_BIT * SUBNET_SUBSCRIPTION_BITS }, \
                           { PACKETBUF_ATTR_EPACKET_TYPE, PACKETBUF_ATTR_BIT }, \
                           { PACKETBUF_ADDR_ERECEIVER, PACKETBUF_ADDRSIZE }, \
                             ADISCLOSE_ATTRIBUTES
/*---------------------------------------------------------------------------*/
/**
 * \brief Subscription identifier
 */
struct subscription {
  const rimeaddr_t *sink;
  short subid;
};
/*---------------------------------------------------------------------------*/
struct subnet_callbacks {
  /* called if no next hop can be contacted */
  void (* errpub)(struct subnet_conn *c);

  /* called when a publish is received */
  void (* ondata)(struct subnet_conn *c, struct subscription *s);

  /* called when a publish was sent successfully */
  void (* onsent)(struct subnet_conn *c, struct subscription *s);

  /* called when a new subscription is in packetbuf */
  void (* subscribe)(struct subnet_conn *c, struct subscription *s);

  /* called when a subscription is cancelled */
  void (* unsubscribe)(struct subnet_conn *c, struct subscription *s);

  /* should return true if the given subscription is known */
  bool (* exists)(struct subnet_conn *c, struct subscription *s);

  /* should fill packetbuf with information about the given subscription */
  void (* inform)(struct subnet_conn *c, struct subscription *s);
};
/*---------------------------------------------------------------------------*/
/**
 * \brief Information about a single next hop
 */
struct sink_route_hop {
  rimeaddr_t nexthop;
  short cost; /* advertised cost for this hop by next hop */
  uint8_t last_active; /* last time this next hop was heard from */
};

/**
 * \brief Information about next hops to a single sink
 */
struct sink_route {
  rimeaddr_t sink;
  short advertised_cost; /* what cost we've advertised this route as */
  struct sink_route_hop[SUBNET_MAX_ALTERNATE_ROUTES];
};

/**
 * \brief Subnet connection state
 */
struct subnet_conn {
  struct adisclose_conn pubsub;     /* connection for pub/sub messages */
  struct adisclose_conn peer;       /* connection for P2P subscription info */
  const struct subnet_callbacks *u; /* callbacks */
  short subid;                      /* last sent subscription id */

  short numroutes;                  /* number of routes (i.e. sinks) known */
  struct sink_route routes[SUBNET_MAX_SINKS];
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
 * \brief Publish the data in packetbuf for the given subscription
 * \param c Connection state
 * \param s Subscription identifier
 */
void subnet_publish(struct subnet_conn *c, const struct subscription *s);

/**
 * \brief Send out a new subscription
 * \param c Connection state
 * \return The subscription id of the new subscription
 */
short subnet_subscribe(struct subnet_conn *c);

/**
 * \brief Send out a new subscription replacing the given subscription
 * \param c Connection state
 * \param subid Subscription to replace
 * \return The subscription id of the new subscription
 */
short subnet_replace(struct subnet_conn *c, short subid);

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
