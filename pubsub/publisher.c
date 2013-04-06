/**
 * \addtogroup pubsub
 * @{
 */

/**
 * \file   Publisher node implementation
 * \author Jon Gjengset <jon@tsp.io>
 */
#include "lib/pubsub/publisher.h"

/*---------------------------------------------------------------------------*/
/* private members */
static struct process *etarget;
/* #define PUBSUB_MAX_SUBSCRIPTIONS */
/*---------------------------------------------------------------------------*/
/* private functions */
void on_errpub(struct subnet_conn *c);
void on_ondata(struct subnet_conn *c, const rimeaddr_t *sink, short subid, void *data);
void on_onsent(struct subnet_conn *c, const rimeaddr_t *sink, short subid);
void on_change();
void on_timer_expired();
void set_needs(reading_type t, bool yes);
size_t get_sizeof(reading_type t);
/*---------------------------------------------------------------------------*/
/* public function definitions */
void publisher_start() {
  etarget = PROCESS_CURRENT();
  pubsub_init(on_errpub, on_ondata, on_onsent, on_change);
}
void publisher_always_has(reading_type t, void *reading, size_t sz);
void publisher_has(reading_type t, size_t sz);
bool publisher_in_need() {
  return numneeds > 0;
}
bool publisher_needs(reading_type t) {
  return needs.contains(t);
}
void publisher_publish(reading_type t, void *reading) {
  pubsub_add_data(/*subid,*/ reading, get_sizeof(t));
  set_needs(n, false);
  if (!publisher_in_need()) {
    pubsub_publish();
  }
}
/*---------------------------------------------------------------------------*/
/* private function definitions */
void on_timer_expired() {
  pubsub_prepublish(/*sink*/);
  for (n in needs) {
    set_needs(n, true);
  }
  process_post(etarget, PROCESS_EVENT_PUBLISH /*, data*/);
}
/*---------------------------------------------------------------------------*/
/** @} */
