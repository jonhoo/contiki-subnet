/**
 * \addtogroup lib
 * @{
 */
/**
 * \defgroup pubsub Publish-Subscribe library
 * @{
 *
 * Uses the subnet networking protocol to build a flexible multi-hop
 * publish-subscribe style sensor network.
 */

/**
 * \file   Header file for the Publish-Subscribe library
 * \author Jon Gjengset <jon@tsp.io>
 */

#ifndef __PUBSUB_H__
#define __PUBSUB_H__
#include "net/rime/subnet.h"
#include "net/rime/rimeaddr.h"
#include <stdbool.h>
#include "pubsub-config.h"
/*---------------------------------------------------------------------------*/
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

#ifdef PUBSUB_CONF_MAX_SUBSCRIPTIONS
#define PUBSUB_MAX_SUBSCRIPTIONS PUBSUB_CONF_MAX_SUBSCRIPTIONS
#else
#define PUBSUB_MAX_SUBSCRIPTIONS 32
#endif
/*---------------------------------------------------------------------------*/
struct subscription {
  clock_time_t      interval;
  enum soft_filter  soft_filter;
  enum hard_filter  hard_filter;
  enum aggregator   aggregator;
  enum reading_type sensor;
  /* TODO: Add params for soft/hard filter. Need for example for van location */
};
struct full_subscription {
  short subid;
  int sink;
  clock_time_t revoked;
  struct subscription in;
};
struct pubsub_state {
  struct subnet_conn c;
  struct pubsub_callbacks *u;
};
struct pubsub_callbacks {
  /* TODO: Avoid struct subnet_conn parameter in these */

  /* Function to call if a publish couldn't be sent */
  void (* on_errpub)(struct subnet_conn *c);

  /* Function to call when a publish is received */
  void (* on_ondata)(struct subnet_conn *c, int sink, short subid, void *data);

  /* Function to call when a publish was sent successfully */
  void (* on_onsent)(struct subnet_conn *c, int sink, short subid);

  /* Function to call when a new subscription was found */
  void (* on_subscription)(struct full_subscription *s);

  /* Function to call when a subscription was removed. Note that after this
   * function returns, the object pointed to will be replaced */
  void (* on_unsubscription)(struct full_subscription *s);
};
/*---------------------------------------------------------------------------*/
/**
 * \brief Initialize a pubsub network
 * \param u Callbacks
 */
void pubsub_init(struct pubsub_callbacks *u);

/**
 * \brief Find information about a given subscription
 * \param sink The subscription's sink
 * \param subid The subscription's ID
 * \return The found subscription or NULL if the subscription is unknown
 */
struct full_subscription * find_subscription(int sink, short subid);

/**
 * \brief Get all known subscriptions
 * \param ss Pointer to make point to array of subscriptions
 * \return Number of known subscriptions
 */
int pubsub_get_subscriptions(struct full_subscription **ss);

/**
 * \brief Add data for a subscription to the current publish
 * \param sinkid Sink to send data to
 * \param subid Subscription data is being added for
 * \param payload Data
 * \param bytes Number of bytes of data being added
 * \return True if data was added, false if no more data can be added
 */
bool pubsub_add_data(int sinkid, short subid, void *payload, size_t bytes);

/**
 * \brief Send publishe data packet
 * \param sink Sink to send data to
 */
void pubsub_publish(int sinkid);

/**
 * \brief Send out a new subscription
 * \param s Subscription to add
 * \return The subscription id of the new subscription
 */
short pubsub_subscribe(struct subscription *s);

/**
 * \brief End the given subscription
 * \param subid Subscription to remove
 */
void pubsub_unsubscribe(short subid);
/*---------------------------------------------------------------------------*/

#endif /* __PUBSUB_H__ */
/** @} */
/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
