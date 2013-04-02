#include "math.h"

enum reading_type {
  READING_LOCATION;
  READING_HUMIDITY;
  READING_PRESSURE;
};

enum cmp_operator {
  DISTANCE_LTE;
  DISTANCE_LT;
  DISTANCE_GT;
  DISTANCE_GTE;
  LTE;
  LT;
  GT;
  GTE;
  BETWEEN;
};

struct filter {
  short is_required;
  reading_type rt;
  cmp_operator op;
  void * a;
  void * b;
}

struct subscription {
  rimeaddr_t sink;
  int sid;
  int count;
  struct filter ** filters;
}

struct reading {
  void *reading;
  reading_type type;
}

struct readings {
  rimeaddr_t node;
  int count;
  struct reading ** readings;
}

short reading_satisfies(reading_type rt, void * reading, struct filter * filter) {
  if (rt != filter->rt) {
    return 0;
  }

  switch (rt) {
    case READING_LOCATION:
      struct location *l = reading;
      struct location *d = *(filter->a);
      double r = sqrt(pow(l->x - d->x, 2) + pow(l->y - d->y, 2));
      double o = *(filter->b);
      switch (filter->op) {
        case CMP_DISTANCE_GTE:
          return r>=o;
        case CMP_DISTANCE_GT:
          return r>o;
        case CMP_DISTANCE_LT:
          return r<o;
        case CMP_DISTANCE_LTE:
          return r<=o;
      }
      break;
    case READING_HUMIDITY:
    case READING_PRESSURE:
      double r = *reading;
      double o = *(filter->a);
      switch (filter->op) {
        case CMP_GTE:
          return r>=o;
        case CMP_GT:
          return r>o;
        case CMP_LT:
          return r<o;
        case CMP_LTE:
          return r<=o;
        case CMP_BETWEEN:
          double m = *(filter->b);
          return r>o && r<m;
      }
      break;
  }

  return 0;
}
