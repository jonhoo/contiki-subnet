/**
 * \addtogroup pubsub
 * @{
 *
 * Publisher nodes are nodes that produce readings.
 */

/**
 * \file   Header file for a publisher node
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
 */
void publisher_start();

/**
 * \brief Sets a static reading for this node that will be included in all readings
 * \param t The type of reading
 * \param reading The value of the reading
 * \param sz The size of the read value
 */
void publisher_always_has(reading_type t, void *reading, size_t sz);

/**
 * \brief Indicates that this node can produce readings of the given type
 * \param t Supported reading type
 * \param sz Size of this reading type
 */
void publisher_has(reading_type t, size_t sz);

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
bool publisher_needs(reading_type t);

/**
 * \brief Publish a new value for the given reading
 * \param t Type of the reading
 * \param reading Value of the reading
 */
void publisher_publish(reading_type t, void *reading);

#endif /* __PUBSUB_PUB_H__ */
/** @} */
/*
 * vim:syntax=cpp.doxygen:
 */
