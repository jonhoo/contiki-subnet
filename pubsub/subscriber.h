/**
 * \addtogroup pubsub
 * @{
 *
 * Subscriber nodes are nodes that want to hear readings.
 */

/**
 * \file   Header file for subscriber nodes
 * \author Jon Gjengset <jon@tsp.io>
 */

#ifndef __PUBSUB_SUB_H__
#define __PUBSUB_SUB_H__
#include "lib/pubsub/common.h"

#ifdef PUBSUB_CONF_RESEND_INTERVAL
#define PUBSUB_RESEND_INTERVAL PUBSUB_CONF_RESEND_INTERVAL
#else
#define PUBSUB_RESEND_INTERVAL 30
#endif
/*---------------------------------------------------------------------------*/
/**
 * \brief Starts the pubsub network connection
 */
void subscriber_start(void (*on_reading)(short subid, void *data));

/**
 * \brief Starts a subscription with the given parameters
 * \return Subscription id of the new subscription
 */
short subscriber_subscribe(struct subscription *s);

/**
 * \brief Replaces a subscription with one with the given parameters
 * \param subid Subscription to replace
 * \return Subscription id of the new subscription
 */
short subscriber_replace(short subid, struct subscription *s);

/**
 * \brief Ends the given subscription
 * \param subid ID of subscription to end
 */
void subscriber_unsubscribe(short subid);

/**
 * \brief Get information about the given subscription
 * \return The subscription identified by the given subscription id or NULL
 */
const struct subscription *subscriber_subscription(short subid);

/* TODO: Add nuke_sink() */
/*---------------------------------------------------------------------------*/

#endif /* __PUBSUB_SUB_H__ */
/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
