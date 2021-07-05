CONTIKI_PROJECT = coap-example-client
# coap-example-observe-client is outdated but will be ported at a later point
all: $(CONTIKI_PROJECT)

# Do not try to build on Sky because of code size limitation
PLATFORMS_EXCLUDE = sky z1

ifneq ($(WCIPHER),0)
CFLAGS += -DWITHOUTCIPHER=$(WCIPHER)
#$(shell touch ./resources/res_push.c)
#$(shell touch project-conf.h)
#PROJECT_SOURCEFILES += resources/res_push.c
${info withouthcipher, ${CFLAGS}}
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
