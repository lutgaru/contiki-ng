CONTIKI_PROJECT = coap-example-server
all: $(CONTIKI_PROJECT)

# Do not try to build on Sky because of code size limitation
PLATFORMS_EXCLUDE = sky z1

ifneq ($(NBYTES),0)
CFLAGS += -DNBYTES=$(NBYTES)
#$(shell touch ./resources/res_push.c)
$(shell touch project-conf.h)
#PROJECT_SOURCEFILES += resources/res_push.c
${info with nbytes, ${CFLAGS}}
endif
# Include CoAP resources
MODULES_REL += ./resources
#MAKE_MAC = MAKE_MAC_TSCH
#MODULES += os/services/shell
#MODULES += os/services/orchestra

CONTIKI=../../..
# Include the CoAP implementation
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

include $(CONTIKI)/Makefile.include
