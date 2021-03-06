
ifndef RTE_SDK
$(error RTE_SDK is undefined)
endif

PROJECT_PATH = /home/amax/projects/serverless_nfv

# Default target, can be overriden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(CONFIG_RTE_EXEC_ENV),"linuxapp")
$(error This application can only operate in a linuxapp environment, \
please change the definition of the RTE_TARGET environment variable)
endif

# binary name
APP = serverlessnfv_executor

DPDK_LIBS_HEADER_DIR = $(PROJECT_PATH)/libs/dpdk/includes
EXECUTOR_LIB = $(PROJECT_PATH)/target/libs/

# all source are stored in SRCS-y
SRCS-y := main.c
SRCS-y += $(SRCDIR)/../common/worker.c $(SRCDIR)/../common/utilization.c $(SRCDIR)/../common/tunnel.c $(SRCDIR)/../common/tag.c common.c dispatcher.c manager.c
SRCS-y += $(SRCDIR)/../funcboxes/funcbox_1/funcbox_1.c $(SRCDIR)/../funcboxes/funcbox_1/funcworker_1.c
SRCS-y += $(SRCDIR)/../funcboxes/funcbox_2/funcbox_2.c $(SRCDIR)/../funcboxes/funcbox_2/funcworker_2.c

INC := $(SRCDIR)/includes/$(wildcard *.h)

CFLAGS += $(WERROR_FLAGS) -O3 $(USER_FLAGS)
CFLAGS += -I$(DPDK_LIBS_HEADER_DIR)
CFLAGS += -I$(PROJECT_PATH)/framework/common/includes
CFLAGS += -I$(PROJECT_PATH)/framework/executor/includes
CFLAGS += -I$(PROJECT_PATH)/framework/funcboxes/funcbox_1/includes
CFLAGS += -I$(PROJECT_PATH)/framework/funcboxes/funcbox_2/includes
CFLAGS += -D_GNU_SOURCE
# LDFLAGS += -L$(EXECUTOR_LIB)
LDFLAGS += -lrt -lpthread -L$(EXECUTOR_LIB) -lmydpdk


# for newer gcc, e.g. 4.4, no-strict-aliasing may not be necessary
# and so the next line can be removed in those cases.
EXTRA_CFLAGS += -fno-strict-aliasing

include $(RTE_SDK)/mk/rte.extapp.mk