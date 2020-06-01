ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = flows_pktgen

# all source are stored in SRCS-y
SRCS-y := main.c

CFLAGS += $(WERROR_FLAGS)

ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_main.o += -Wno-return-type
endif

EXTRA_CFLAGS += -O3 -g

include $(RTE_SDK)/mk/rte.extapp.mk
