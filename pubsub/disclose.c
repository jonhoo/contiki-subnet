
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
  const rimeaddr_t *to = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

  PRINTF("disclose: recv_from_broadcast, %d.%d -> %d.%d\n",
      from->u8[0], from->u8[1],
      to->u8[0], to->u8[1]);

  if (rimeaddr_cmp(to, &rimeaddr_node_addr)) {
    PRINTF("disclose: recv.\n");
    c->u->recv(c, from);
  } else {
    PRINTF("disclose: hear.\n");
    c->u->hear(c, from);
  }
}
/*---------------------------------------------------------------------------*/
static void
sent_by_broadcast(struct broadcast_conn *broadcast, int status, int num_tx)
{
  struct disclose_conn *c = (struct disclose_conn *)broadcast;
  const rimeaddr_t *to = packetbuf_addr(PACKETBUF_ADDR_RECEIVER);

  PRINTF("disclose: sent_by_broadcast, receiver %d.%d\n",
      to->u8[0], to->u8[1]);

  if (c->u->sent) {
    c->u->sent(c, status);
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
  if (rimeaddr_cmp(receiver, &rimeaddr_null)) {
    PRINTF("disclose: broadcast\n");
  } else {
    PRINTF("disclose: send to %d.%d\n", receiver->u8[0], receiver->u8[1]);
  }

  packetbuf_set_addr(PACKETBUF_ADDR_RECEIVER, receiver);
  return broadcast_send(&c->c);
}
/*---------------------------------------------------------------------------*/
/** @} */
