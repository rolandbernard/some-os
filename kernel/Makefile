
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
CCFLAGS += -nostdlib -mno-relax -fpic -fpie -ffreestanding
CCFLAGS += -I$(SOURCE_DIR)/libc/include
LINK_SCRIPT = $(SOURCE_DIR)/bsp/$(BOARD)/link.ld
# ==

include ../build.mk

