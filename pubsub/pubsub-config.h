#ifndef __PUBSUB_CONF_H__
#define __PUBSUB_CONF_H__
/*---------------------------------------------------------------------------*/
/* sensor structs */
struct location {
  short x;
  short y;
};
struct locdouble {
  struct location location;
  double value;
};
typedef struct locdouble humidity;
typedef struct locdouble pressure;
/*---------------------------------------------------------------------------*/
/* middleware types */
enum reading_type {
  READING_LOCATION,
  READING_HUMIDITY,
  READING_PRESSURE,
};

enum soft_filter  {
  NO_SOFT_FILTER
};

union soft_arg {
  double reading;
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
  double distance;
};
/*---------------------------------------------------------------------------*/
#endif /* __PUBSUB_CONF_H__ */
