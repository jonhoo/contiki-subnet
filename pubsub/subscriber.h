void subscriber_start(*ss, void (*on_reading)(struct readings *));
void subscriber_subscribe(*ss, reading_type rt, int periodicity, size_t rs);
void * subscriber_data(*ss, rimeaddr_t node, reading_type rt);

subscriber_filter(*ss, reading_type rt, cmp_operator op, void * a, void * b);
subscriber_filter(*ss, reading_type rt, cmp_operator op, void * a);
subscriber_satisfy(*ss, reading_type rt, cmp_operator op, void * a, void * b);
subscriber_satisfy(*ss, reading_type rt, cmp_operator op, void * a);
