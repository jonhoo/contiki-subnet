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
  NO_HARD_FILTER
};

union hard_arg {
  double reading;
};

enum aggregator_t {
  NO_AGGREGATION
};

union aggregator_arg {
  double distance;
};
/*---------------------------------------------------------------------------*/
