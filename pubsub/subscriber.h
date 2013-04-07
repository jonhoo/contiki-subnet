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
/*---------------------------------------------------------------------------*/
/**
 * \brief Starts the pubsub network connection
 */
void subscriber_start(void (*on_reading)(struct readings *));

/**
 * \brief Starts a subscription with the given parameters
 * \return Subscription id of the new subscription
 */
short subscriber_subscribe(struct subscription s);

/**
 * \brief Ends the given subscription
 * \param subid ID of subscription to end
 */
void subscriber_unsubscribe(short subid);
/*---------------------------------------------------------------------------*/

#endif /* __PUBSUB_SUB_H__ */
/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
