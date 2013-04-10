/**
 * \addtogroup rime
 * @{
 */

/**
 * \defgroup rimeadc Single-hop ACKed disclosed unicast
 * @{
 *
 * Works like disclosed unicast, but only reports a successfull send if an ACK
 * is received. Note that unlike runicast, this primitive only sends a single
 * transmission (that is, it is not stubborn; it fails if a single transmission
 * fails).
 *
 * The adisclose primitive adds two packet attributes: the single-hop packet
 * type and the single-hop packet ID. The adisclose primitive uses the packet
 * ID attribute as a sequence number for matching acknowledgement packets to the
 * corresponding data packets.
 *
 *
 * \section channels Channels
 *
 * The adisclose module uses 1 channel.
 *
 */

/**
 * \file
 *         Acknowledged disclosed unicast header file
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#ifndef __ADISCLOSE_H__
#define __ADISCLOSE_H__

#include "sys/ctimer.h"
#include "net/rime/disclose.h"

#define ADISCLOSE_PACKET_ID_BITS 2

#define ADISCLOSE_ATTRIBUTES  { PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_BIT }, \
                              { PACKETBUF_ATTR_PACKET_ID, PACKETBUF_ATTR_BIT * ADISCLOSE_PACKET_ID_BITS }, \
                             DISCLOSE_ATTRIBUTES
struct adisclose_conn {
  struct disclose_conn c;
  const struct adisclose_callbacks *u;
  struct ctimer t;
  uint8_t sndnxt;
  uint8_t is_tx;
  uint8_t failed;
  rimeaddr_t receiver;
};

struct adisclose_callbacks {
  void (* recv)(struct adisclose_conn *c, const rimeaddr_t *from);
  void (* sent)(struct adisclose_conn *c, const rimeaddr_t *to);
  void (* timedout)(struct adisclose_conn *c, const rimeaddr_t *to);
  void (* hear)(struct adisclose_conn *c, const rimeaddr_t *from);
};

void adisclose_open(struct adisclose_conn *c, uint16_t channel,
	       const struct adisclose_callbacks *u);
void adisclose_close(struct adisclose_conn *c);

int adisclose_send(struct adisclose_conn *c, const rimeaddr_t *receiver);

uint8_t adisclose_is_transmitting(struct adisclose_conn *c);

#endif /* __ADISCLOSE_H__ */
/** @} */
/** @} */
