
ifndef RTE_SDK
$(error RTE_SDK is undefined)
endif

PROJECT_PATH = /home/ubuntu/projects/serverless-nfv

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(CONFIG_RTE_EXEC_ENV),"linuxapp")
$(error This application can only operate in a linuxapp environment, \
please change the definition of the RTE_TARGET environment variable)
endif

# binary name
APP = serverlessnfv_gateway

DPDK_LIBS_HEADER_DIR = $(PROJECT_PATH)/libs/dpdk/includes
GATEWAY_LIB = $(PROJECT_PATH)/target/libs/

# all source are stored in SRCS-y
SRCS-y := main.c
SRCS-y += gateway.c $(SRCDIR)/../common/worker.c $(SRCDIR)/../common/tunnel.c $(SRCDIR)/../common/tag.c

INC := $(SRCDIR)/includes/$(wildcard *.h)

#CFLAGS += $(WERROR_FLAGS) -O3 -Wall -m64 -march=native -Wno-unused-function -Wno-unused-but-set-variable
CFLAGS += $(WERROR_FLAGS) -O3 $(USER_FLAGS)
CFLAGS += -I$(DPDK_LIBS_HEADER_DIR)
CFLAGS += -I$(PROJECT_PATH)/framework/common/includes
CFLAGS += -I$(PROJECT_PATH)/framework/gateway_rtc/includes
CFLAGS += -D_GNU_SOURCE
LDFLAGS += -L$(GATEWAY_LIB)
LDFLAGS += -lrt -lpthread -L$(GATEWAY_LIB) -lmydpdk


# for newer gcc, e.g. 4.4, no-strict-aliasing may not be necessary
# and so the next line can be removed in those cases.
EXTRA_CFLAGS += -fno-strict-aliasing

include $(RTE_SDK)/mk/rte.extapp.mk