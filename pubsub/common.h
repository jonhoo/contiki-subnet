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
#define PUBSUB_MAX_SUBSCRIPTIONS 128
#endif
/*---------------------------------------------------------------------------*/
struct subscription {
  uint8_t       period;
  soft_filters  soft_filter;
  hard_filters  hard_filter;
  aggregators   aggregator;
  reading_types sensor;
};
struct full_subscriptions {
  short subid;
  rimeaddr_t sink;
  struct subscription in;
};
struct pubsub_state {
  struct subnet_conn c;
  void (* on_change)();
};
/*---------------------------------------------------------------------------*/
/**
 * \brief Initialize a pubsub network
 * \param on_errpub Function to call if a publish couldn't be sent
 * \param on_ondata Function to call when a publish is received
 * \param on_onsent Function to call when a publish was sent successfully
 * \param on_change Function to call when the list of subscriptions has changed
 */
void pubsub_init(
  void (* on_errpub)(struct subnet_conn *c),
  void (* on_ondata)(struct subnet_conn *c, const rimeaddr_t *sink, short subid, void *data),
  void (* on_onsent)(struct subnet_conn *c, const rimeaddr_t *sink, short subid),
  void (* on_change)()
);

/**
 * \brief Find information about a given subscription
 * \param sink The subscription's sink
 * \param subid The subscription's ID
 * \return The found subscription or NULL if the subscription is unknown
 */
struct subscription * find_subscription(const rimeaddr_t *sink, short subid);

/**
 * \brief Get all known subscriptions
 * \param ss Pointer to make point to array of subscriptions
 * \return Number of known subscriptions
 */
int get_subscriptions(struct full_subscription **ss);
/*---------------------------------------------------------------------------*/

#endif /* __PUBSUB_H__ */
/** @} */
/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
