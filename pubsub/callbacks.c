#include "lib/pubsub.h"

/*---------------------------------------------------------------------------*/
/* proxy callbacks */
bool soft_filter_proxy(struct sfilter *f, enum reading_type t, void *data) {
  return false;
}

bool hard_filter_proxy(struct hfilter *f) {
  return false;
}

void aggregator_proxy(struct aggregator *a, struct full_subscription *s, int items, void *datas[]) {
  return;
}
/*---------------------------------------------------------------------------*/
