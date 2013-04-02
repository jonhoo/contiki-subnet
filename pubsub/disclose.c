
/**
 * \addtogroup rimedc
 * @{
 */

/**
 * \file
 *         Disclosed unicast
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#include "net/rime.h"
#include "disclose.h"
#include <string.h>

static const struct packetbuf_attrlist attributes[] =
  {
    DISCLOSE_ATTRIBUTES
    PACKETBUF_ATTR_LAST
  };

#define DEBUG 0
#if DEBUG
#include <stdio.h>
#define PRINTF(...) printf(__VA_ARGS__)
#else
#define PRINTF(...)
#endif

/*---------------------------------------------------------------------------*/
static void
recv_from_broadcast(struct broadcast_conn *broadcast, const rimeaddr_t *from)
{
  struct disclose_conn *c = (struct disclose_conn *)broadcast;

  PRINTF("%d.%d: dc: recv_from_broadcast, receiver %d.%d\n",
	 rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	 packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
	 packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);
  if(rimeaddr_cmp(packetbuf_addr(PACKETBUF_ADDR_RECEIVER), &rimeaddr_node_addr)) {
    c->u->recv(c, from);
  } else {
    c->u->hear(c, from);
  }
}
/*---------------------------------------------------------------------------*/
static void
sent_by_broadcast(struct broadcast_conn *broadcast, int status, int num_tx)
{
  struct disclose_conn *c = (struct disclose_conn *)broadcast;

  PRINTF("%d.%d: dc: sent_by_broadcast, receiver %d.%d\n",
	 rimeaddr_node_addr.u8[0], rimeaddr_node_addr.u8[1],
	 packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[0],
	 packetbuf_addr(PACKETBUF_ADDR_RECEIVER)->u8[1]);

  if(c->u->sent) {
    c->u->sent(c, status, num_tx);
  }
}
/*---------------------------------------------------------------------------*/
static const struct broadcast_callbacks uc = {recv_from_broadcast,
                                              sent_by_broadcast};
/*---------------------------------------------------------------------------*/
void
disclose_open(struct disclose_conn *c, uint16_t channel,
	      const struct disclose_callbacks *u)
{
  broadcast_open(&c->c, channel, &uc);
  c->u = u;
  channel_set_attributes(channel, attributes);
}
/*---------------------------------------------------------------------------*/
void
disclose_close(struct disclose_conn *c)
{
  broadcast_close(&c->c);
}
/*---------------------------------------------------------------------------*/
int
disclose_send(struct disclose_conn *c, const rimeaddr_t *receiver)
{
  PRINTF("%d.%d: disclose_send to %d.%d\n",
	 rimeaddr_node_addr.u8[0],rimeaddr_node_addr.u8[1],
	 receiver->u8[0], receiver->u8[1]);
  packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, receiver);
  return broadcast_send(&c->c);
}
/*---------------------------------------------------------------------------*/
/** @} */
