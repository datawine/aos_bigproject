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
APP = funcbox_0

DPDK_LIBS_HEADER_DIR = $(PROJECT_PATH)/libs/dpdk/includes
TARGET_LIBS = $(PROJECT_PATH)/target/libs
NFS_LIBS = $(PROJECT_PATH)/target/nfs

# all source are stored in SRCS-y
SRCS-y := funcbox_0.c funcworker_0.c
SRCS-y += $(wildcard $(SRCDIR)/../../common/*.c)
#SRCS-y += $(SRCDIR)/../../common/worker.c $(SRCDIR)/../../common/utilization.c
#SRCS-y += $(SRCDIR)/../../common/tunnel.c $(SRCDIR)/../../common/tag.c

INC := $(SRCDIR)/includes/$(wildcard *.h)

CFLAGS += $(WERROR_FLAGS) -O3 $(USER_FLAGS)
CFLAGS += -I$(DPDK_LIBS_HEADER_DIR)
CFLAGS += -I$(PROJECT_PATH)/framework/common/includes
CFLAGS += -I$(PROJECT_PATH)/nfs/includes
CFLAGS += -I$(PROJECT_PATH)/framework/funcboxes/funcbox_0/includes
CFLAGS += -D_GNU_SOURCE
LDFLAGS += -lrt -ldpdk -L$(NFS_LIBS) -lnfs -L$(TARGET_LIBS) -lmydpdk -lhiredis


# for newer gcc, e.g. 4.4, no-strict-aliasing may not be necessary
# and so the next line can be removed in those cases.
EXTRA_CFLAGS += -fno-strict-aliasing

include $(RTE_SDK)/mk/rte.extapp.mk