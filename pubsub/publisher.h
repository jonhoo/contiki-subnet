publisher_start(*ps, *on_subscription);
publisher_always_has(*ps, reading_type, *reading, reading_size);
publisher_has(*ps, reading_type, reading_size);
publisher_needs(*ps);
publisher_needs(*ps, reading_type);
publisher_publish(*ps, reading_type, *reading);
