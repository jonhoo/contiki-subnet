/**
 * \addtogroup pubsub
 * @{
 */

/**
 * \file   Subscriber node implementation
 * \author Jon Gjengset <jon@tsp.io>
 */
#include "lib/pubsub/subscriber.h"

/*---------------------------------------------------------------------------*/
/* private members */
/*---------------------------------------------------------------------------*/
/* private functions */
/*---------------------------------------------------------------------------*/
/* public function definitions */
/*---------------------------------------------------------------------------*/
/* for reference */
short subnet_subscribe(struct subnet_conn *c, void *payload, size_t bytes);
void subnet_unsubscribe(struct subnet_conn *c, short subid);
/*---------------------------------------------------------------------------*/
/** @} */
