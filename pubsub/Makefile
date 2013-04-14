CONTIKI = ..

CONTIKI_PROJECT = node sink van plain

CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

all: $(CONTIKI_PROJECT)
	msp430-strip --strip-debug node.sky
	msp430-strip --strip-debug van.sky
	msp430-strip --strip-debug sink.sky
	msp430-strip --strip-debug plain.sky

include $(CONTIKI)/Makefile.include
