
include ../config.mk

# == Config
TARGETS := init hello systest
TARGETS += ls basename dirname rm mv
# ==

# == Tools
LD := $(ARCH)-someos-gcc
CC := $(ARCH)-someos-gcc
#==

# == Flags
CCFLAGS += -I$(SYSROOT_DIR)/include
CCJFLAGS += -D__someos__ --target=riscv64
# ==

include ../build.mk

