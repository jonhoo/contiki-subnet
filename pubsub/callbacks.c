#include "lib/pubsub.h"
#include "pubsub-config.h"
#include <stdlib.h>

/*---------------------------------------------------------------------------*/
/* values to be defined on nodes */
struct location node_location;
/*---------------------------------------------------------------------------*/
/* proxy callbacks */
bool soft_filter_proxy(struct sfilter *f, enum reading_type t, void *data) {
  return false;
}

bool hard_filter_proxy(struct hfilter *f) {
  struct location *v, *n;
  switch (f->filter) {
    case BE_CLOSE_TO:
      v = &f->arg.loc;
      n = &node_location;

      if (abs(n->x - v->x) > 10) return true;
      if (abs(n->y - v->y) > 10) return true;
      /* fall-through */
    default:
      return false;
  }
}

void aggregator_proxy(struct aggregator *a, struct full_subscription *s, int items, void *datas[]) {
  return;
}
/*---------------------------------------------------------------------------*/
