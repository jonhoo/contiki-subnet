/**
 * \addtogroup pubsub
 * @{
 *
 * Publisher nodes are nodes that produce readings.
 */

/**
 * \file   Header file for publisher nodes
 * \author Jon Gjengset <jon@tsp.io>
 */

#ifndef __PUBSUB_PUB_H__
#define __PUBSUB_PUB_H__
#include "lib/pubsub/common.h"

#ifdef PUBSUB_CONF_MAX_SENSORS
#define PUBSUB_MAX_SENSORS PUBSUB_CONF_MAX_SENSORS
#else
#define PUBSUB_MAX_SENSORS 5
#endif

/**
 * \brief Starts the pubsub network connection
 * \param soft_filter_proxy Function to use as a soft filter proxy. Should
 *          return true if the value should be filtered. Note that data may be a
 *          NULL pointer if the node doesn't have the given sensor
 * \param hard_filter_proxy Function to use as a hard filter proxy. Should
 *          return true if the value should be filtered. Note that data may be a
 *          NULL pointer if the node doesn't have the given sensor
 */
void publisher_start(
  bool (* soft_filter_proxy)(enum soft_filter f, enum reading_type t, void *data, void *arg),
  bool (* hard_filter_proxy)(enum hard_filter f, enum reading_type t, void *data, void *arg)
);

/**
 * \brief Indicates that this node can produce readings of the given type
 * \param t Supported reading type
 * \param sz Size of this reading type
 */
void publisher_has(enum reading_type t, size_t sz);

/**
 * \brief Determine if readings are needed
 * \return true if readings are needed, false otherwise
 */
bool publisher_in_need();

/**
 * \brief Determine if the given reading is needed
 * \param t Type of reading
 * \return true if a new reading is needed, false otherwise
 */
bool publisher_needs(enum reading_type t);

/**
 * \brief Publish a new value for the given reading
 * \param t Type of the reading
 * \param reading Value of the reading
 */
void publisher_publish(enum reading_type t, void *reading);

#endif /* __PUBSUB_PUB_H__ */
/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
