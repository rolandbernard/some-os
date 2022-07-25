
include ../config.mk

# == Config
TARGETS      := kernel
SWITCHES     := $(ARCH) $(BOARD)
ALL_SWITCHES := riscv64 virt
# ==

# == Tools
LD := $(ARCH)-someos-ld
CC := $(ARCH)-someos-gcc
#==

# == Flags
CCFLAGS += -nostdlib -ffreestanding -mcmodel=medany
CCFLAGS += -I$(SOURCE_DIR)/libc/include
LDFLAGS += -L$(TOOLS_DIR)/lib/gcc/riscv64-someos/12.0.0/
LDLIBS  += -lgcc
LINK_SCRIPT = $(SOURCE_DIR)/bsp/$(BOARD)/link.ld
# ==

include ../build.mk

