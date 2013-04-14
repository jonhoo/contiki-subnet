#ifndef __PUBSUB_CONF_H__
#define __PUBSUB_CONF_H__
/*---------------------------------------------------------------------------*/
/* sensor structs */
struct location {
  short x;
  short y;
};
struct locshort {
  struct location location;
  short value;
};
typedef struct locshort humidity;
typedef struct locshort pressure;
/*---------------------------------------------------------------------------*/
/* middleware types */
enum reading_type {
  READING_HUMIDITY,
  READING_PRESSURE,
};

enum soft_filter  {
  NO_SOFT_FILTER,
  DEVIATION
};

union soft_arg {
  short deviation;
};

enum hard_filter {
  NO_HARD_FILTER,
  BE_CLOSE_TO
};

union hard_arg {
  struct location loc;
};

enum aggregator_t {
  NO_AGGREGATION
};

union aggregator_arg {
  short distance;
};
/*---------------------------------------------------------------------------*/
#endif /* __PUBSUB_CONF_H__ */
