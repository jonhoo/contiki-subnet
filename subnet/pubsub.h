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
#include "subnet-config.h"
/*---------------------------------------------------------------------------*/
#ifdef PUBSUB_CONF_MAX_SUBSCRIPTIONS
#define PUBSUB_MAX_SUBSCRIPTIONS PUBSUB_CONF_MAX_SUBSCRIPTIONS
#else
#define PUBSUB_MAX_SUBSCRIPTIONS 8
#endif
/*---------------------------------------------------------------------------*/
struct sfilter {
  enum soft_filter filter;
  union soft_arg   arg;
};
struct hfilter {
  enum hard_filter filter;
  union hard_arg   arg;
};
struct aggregator {
  enum aggregator_t    aggregator;
  union aggregator_arg arg;
};
struct subscription {
  clock_time_t      interval;
  struct sfilter    soft;
  struct hfilter    hard;
  struct aggregator aggregator;
  enum reading_type sensor;
};
struct esubscription {
  clock_time_t revoked;
  struct subscription in;
};
struct wsubscription {
  short sink;
  subid_t subid;
  struct esubscription *esub;
};
struct pubsub_state {
  struct subnet_conn c;
  struct pubsub_callbacks *u;
};
struct pubsub_callbacks {
  /* Function to call if a publish couldn't be sent */
  void (* on_errpub)();

  /* Function to call when a publish is received */
  void (* on_ondata)(short sink, subid_t subid, void *data);

  /* Function to call when a new subscription was found */
  void (* on_subscription)(struct esubscription *s);

  /* Function to call when a subscription was removed. Note that after this
   * function returns, the object pointed to will be replaced */
  void (* on_unsubscription)(struct esubscription *s);
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
struct esubscription * find_subscription(short sink, subid_t subid);

/**
 * \brief Determine if the given subscription is active
 * \param s Subscription
 * \return True if subscription is KNOWN, false otherwise
 */
bool is_active(struct esubscription *s);

/**
 * \brief Make the given pointer point to the next subscription
 * \param prev Pointer to subscription to update. .sink should be set to -1
 *             before calling this function
 * \return True if a next subscription was found, false otherwise
 */
bool pubsub_next_subscription(struct wsubscription *rev);

/**
 * \brief Add data for a subscription to the current publish
 * \param sinkid Sink to send data to
 * \param subid Subscription data is being added for
 * \param payload Data
 * \param bytes Number of bytes of data being added
 * \return True if data was added, false if no more data can be added
 */
bool pubsub_add_data(short sinkid, subid_t subid, void *payload, dlen_t bytes);

/**
 * \brief Send publishe data packet
 * \param sink Sink to send data to
 */
void pubsub_publish(short sinkid);

/**
 * \brief Send out a new subscription
 * \param s Subscription to add
 * \return The subscription id of the new subscription
 */
subid_t pubsub_subscribe(struct subscription *s);

/**
 * \brief Send out a subscription again
 * \param subid Subscription to resend
 */
void pubsub_resubscribe(subid_t subid);

/**
 * \brief Get current amount of data queued for the given sink
 * \param sink Sink to check payload size for
 * \return Number of bytes queued, -1 on error
 */
short pubsub_packetlen(short sinkid);

/**
 * \brief End the given subscription
 * \param subid Subscription to remove
 */
void pubsub_unsubscribe(subid_t subid);

/**
 * \brief Returns this node's sink id
 * \return this node's sink id or -1 if not known
 *
 * Note that this function will only return something sensible after the first
 * subscription has been sent out!
 */
short pubsub_myid();

/**
 * \brief Redirect all writes to the given sink to a spare buffer
 * \param sinkid Sink to redirect writes for
 *
 * Note that only a single buffer is available per buffer, so this function can
 * only be active for one sink at the time.
 */
void pubsub_writeout(short sinkid);

/**
 * \brief Write all data from the spare buffer into the sink's data
 */
void pubsub_writein();

/**
 * \brief Find all published values for the given subscription
 * \param sink The sink of the subscription to find values for
 * \param subid The subscription to find values for
 * \param payloads Array into which to load pointers to values
 * \param plen Number of elements payloads[] can hold
 * \return The number of values extracted
 *
 * This function will fill the passed array with pointers directly to each
 * reading payload it finds in the sink's current packet buffer.
 *
 * If you use this data to write data to the packetbuf, you might want to look
 * at wrapping it in pubsub_writeout() and pubsub_writein().
 */
uint8_t extract_data(short sink, subid_t subid, void *payloads[], int plen);

/**
 * \brief Find the highest known subscription id for the given sink
 * \param sink The sink id
 * \return the highest known subscription id for the given sink
 */
subid_t last_subscription(short sink);

/**
 * \brief End all subscriptions and close subnet connection
 *
 * Note that this function *MUST* be called before a sink quits to free up
 * resources in the network, otherwise the sink's later subscriptions may be
 * ignored!
 */
void pubsub_close();
/*---------------------------------------------------------------------------*/

#endif /* __PUBSUB_H__ */
/** @} */
/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
