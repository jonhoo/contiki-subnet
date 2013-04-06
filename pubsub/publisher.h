#include "net/rime/subnet.h"
#include "lib/pubsub/common.h"

struct publisher {
  struct subnet_conn c;
  struct process *caller;
};

void publisher_start(*ps, *on_subscription);
void publisher_always_has(*ps, reading_type, *reading, reading_size);
void publisher_has(*ps, reading_type, reading_size);
void publisher_in_need(*ps);
void publisher_needs(*ps, reading_type);
void publisher_publish(*ps, reading_type, *reading);

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

