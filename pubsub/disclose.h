/**
 * \addtogroup rime
 * @{
 */

/**
 * \defgroup rimedc Single-hop disclosed unicast
 * @{
 *
 * The disclose module sends a packet to an identified single-hop neighbor, but
 * also invokes a callback on all other single-hop neighbors.  The disclose
 * primitive uses the broadcast primitive and adds the single-hop receiver
 * address attribute to the outgoing packets. For incoming packets, the unicast
 * module inspects the single-hop receiver address attribute and calls different
 * callback functions depending on whether the address matches the address of
 * the node.
 *
 * \section channels Channels
 *
 * The disclose module uses 1 channel.
 *
 */

/**
 * \file
 *         Header file for Rime's disclosed unicast
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#ifndef __DISCLOSE_H__
#define __DISCLOSE_H__


#include "net/rime/broadcast.h"

#define DISCLOSE_ATTRIBUTES   { PACKETBUF_ADDR_RECEIVER, PACKETBUF_ADDRSIZE }, \
                        BROADCAST_ATTRIBUTES

struct disclose_callbacks {
  void (* recv)(struct disclose_conn *c, const rimeaddr_t *from);
  void (* hear)(struct disclose_conn *c, const rimeaddr_t *from);
  void (* sent)(struct disclose_conn *ptr, int status, int num_tx);
};

struct disclose_conn {
  struct broadcast_conn c;
  const struct disclose_callbacks *u;
};

void disclose_open(struct disclose_conn *c, uint16_t channel,
	      const struct disclose_callbacks *u);
void disclose_close(struct disclose_conn *c);

int disclose_send(struct disclose_conn *c, const rimeaddr_t *receiver);

#endif /* __DISCLOSE_H__ */
/** @} */
/** @} */
