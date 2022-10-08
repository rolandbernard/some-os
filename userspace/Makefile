
include ../config.mk

# == Config
TARGETS := init hello systest
TARGETS += ls basename dirname rm mv cp cat tee
TARGETS += chmod sleep stat chown head tail touch
TARGETS += mkdir rmdir wc date cmp env ln link seq
TARGETS += unlink find grep sort edit clear expr
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

