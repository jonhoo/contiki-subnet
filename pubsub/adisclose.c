/**
 * \addtogroup rimeadc
 * @{
 */


/**
 * \file
 *         Acknowledged disclosed unicast
 * \author
 *         Jon Gjengset <jon@tsp.io>
 */

#include "net/rime/adisclose.h"
#include "net/rime.h"
#include <string.h>

#ifdef ADISCLOSE_CONF_TIMEOUT_TIME
#define TIMEOUT_TIME ADISCLOSE_CONF_TIMEOUT_TIME
#else
#define TIMEOUT_TIME (CLOCK_SECOND/2)
#endif

static const struct packetbuf_attrlist attributes[] =
  {
    ADISCLOSE_ATTRIBUTES
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
recv_from_disclose(struct disclose_conn *disclose, const rimeaddr_t *from)
{
  struct adisclose_conn *c = (struct adisclose_conn *)disclose;

  PRINTF("adisclose: recv_from_disclose from %d.%d type %d seqno %d\n",
         from->u8[0], from->u8[1],
         packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE),
         packetbuf_attr(PACKETBUF_ATTR_PACKET_ID));

  if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) == PACKETBUF_ATTR_PACKET_TYPE_ACK) {

    PRINTF("adisclose: got ACK from %d.%d, seqno %d (%d)\n",
       from->u8[0], from->u8[1],
       packetbuf_attr(PACKETBUF_ATTR_PACKET_ID),
       c->sndnxt);

    if (packetbuf_attr(PACKETBUF_ATTR_PACKET_ID) == c->sndnxt) {
      ctimer_stop(&c->t);
      if (c->failed) {
        // We were too late, timedout() has already been called
        return;
      }

      RIMESTATS_ADD(ackrx);
      PRINTF("adisclose: ACKed %d\n", packetbuf_attr(PACKETBUF_ATTR_PACKET_ID));
      c->sndnxt = (c->sndnxt + 1) % (1 << ADISCLOSE_PACKET_ID_BITS);
      c->is_tx = 0;
      if (c->u->sent != NULL) {
        c->u->sent(c, &c->receiver);
      }
    } else {
      PRINTF("adisclose: received bad ACK %d for %d\n",
             packetbuf_attr(PACKETBUF_ATTR_PACKET_ID),
             c->sndnxt);
      RIMESTATS_ADD(badackrx);
    }
  } else if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) == PACKETBUF_ATTR_PACKET_TYPE_DATA) {
    uint16_t packet_seqno;
    struct queuebuf *q;

    RIMESTATS_ADD(reliablerx);

    packet_seqno = packetbuf_attr(PACKETBUF_ATTR_PACKET_ID);
    PRINTF("adisclose: got packet %d\n", packet_seqno);

    q = queuebuf_new_from_packetbuf();
    if (q != NULL) {
      PRINTF("adisclose: sending ACK to %d.%d for %d\n",
             from->u8[0], from->u8[1],
             packet_seqno);
      packetbuf_clear();
      packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_PACKET_TYPE_ACK);
      packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, packet_seqno);
      disclose_send(&c->c, from);
      RIMESTATS_ADD(acktx);

      queuebuf_to_packetbuf(q);
      queuebuf_free(q);
    } else {
      PRINTF("adisclose: could not send ACK to %d.%d for %d: no queued buffers\n",
             from->u8[0], from->u8[1],
             packet_seqno);
    }

    if (c->u->recv != NULL) {
      c->u->recv(c, from);
    }
  }
}
/*---------------------------------------------------------------------------*/
static void
timeout(void *ptr)
{
  struct adisclose_conn *c = ptr;
  c->failed = 1;
  c->is_tx = 0;
  RIMESTATS_ADD(timedout);

  PRINTF("adisclose: packet %d timed out\n", c->sndnxt);
  c->sndnxt = (c->sndnxt + 1) % (1 << ADISCLOSE_PACKET_ID_BITS);

  if(c->u->timedout) {
    c->u->timedout(c, &c->receiver);
  }
}
/*---------------------------------------------------------------------------*/
static void
hear_from_disclose(struct disclose_conn *disclose, const rimeaddr_t *from)
{
  struct adisclose_conn *c = (struct adisclose_conn *)disclose;

  if (packetbuf_attr(PACKETBUF_ATTR_PACKET_TYPE) == PACKETBUF_ATTR_PACKET_TYPE_DATA) {

    if (c->u->hear != NULL) {
      c->u->hear(c, from);
    }
  }
}
/*---------------------------------------------------------------------------*/
static const struct disclose_callbacks adisclose = {recv_from_disclose,
						    hear_from_disclose, NULL};
/*---------------------------------------------------------------------------*/
void
adisclose_open(struct adisclose_conn *c, uint16_t channel,
	  const struct adisclose_callbacks *u)
{
  disclose_open(&c->c, channel, &adisclose);
  channel_set_attributes(channel, attributes);
  c->u = u;
  c->is_tx = 0;
  c->sndnxt = 0;
}
/*---------------------------------------------------------------------------*/
void
adisclose_close(struct adisclose_conn *c)
{
  disclose_close(&c->c);
}
/*---------------------------------------------------------------------------*/
uint8_t
adisclose_is_transmitting(struct adisclose_conn *c)
{
  return c->is_tx;
}
/*---------------------------------------------------------------------------*/
int
adisclose_send(struct adisclose_conn *c, const rimeaddr_t *receiver)
{
  int ret;
  if(adisclose_is_transmitting(c)) {
    PRINTF("adisclose: already transmitting\n");
    return 0;
  }

  ctimer_set(&c->t, TIMEOUT_TIME, timeout, c);
  rimeaddr_copy(&c->receiver, receiver);

  packetbuf_set_attr(PACKETBUF_ATTR_PACKET_TYPE, PACKETBUF_ATTR_PACKET_TYPE_DATA);
  packetbuf_set_attr(PACKETBUF_ATTR_PACKET_ID, c->sndnxt);
  c->is_tx = 1;
  RIMESTATS_ADD(reliabletx);
  PRINTF("adisclose: sending packet %d\n", c->sndnxt);
  ret = disclose_send(&c->c, receiver);
  if(!ret) {
    c->is_tx = 0;
    c->failed = 0;
  }
  return ret;
}
/*---------------------------------------------------------------------------*/
/** @} */
