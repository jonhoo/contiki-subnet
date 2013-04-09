#include "math.h"

enum reading_type {
  READING_LOCATION,
  READING_HUMIDITY,
  READING_PRESSURE,
};

enum soft_filter  {
  SOFT_FILTER_1
};

union soft_arg {
  double reading;
};

enum hard_filter {
  HARD_FILTER_1
};

union hard_arg {
  double reading;
};

enum aggregator_t {
  AGGREGATOR_1
};

union aggregator_arg {
  double distance;
};
