
include ../config.mk

# == Config
TARGETS      := kernel
SWITCHES     := $(ARCH) $(BOARD) profile
ALL_SWITCHES := riscv64 virt profile
# ==

# == Tools
LD := $(ARCH)-someos-ld
CC := $(ARCH)-someos-gcc
#==

# == Flags
CCFLAGS += -nostdlib -ffreestanding -mcmodel=medany
CCFLAGS += -I$(SOURCE_DIR)/libc/include
CCFLAGS += -DCRITICAL_FAST_PATH

LDFLAGS += -L$(TOOLS_DIR)/lib/gcc/riscv64-someos/12.0.0/
LDLIBS  += -lgcc

LINK_SCRIPT = $(SOURCE_DIR)/bsp/$(BOARD)/link.ld

CCFLAGS.debug += -funwind-tables
# ==

# == Custom rules
PREBUILD.debug += $(OBJECT_DIR)/debuginfo.gen.o
# ==

include ../build.mk

# == Custom rules
$(BINARY_DIR)/kernel-nortdebug: $(OBJECTS) $$(TARGET_OBJECTS.kernel) $(LINK_SCRIPT) | $$(dir $$@)
	@$(ECHO) "Building $@"
	$(RM) PREBUILD.debug
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(TARGET_OBJECTS.kernel) $(LDLIBS) $(if $(LINK_SCRIPT), -T$(LINK_SCRIPT))

$(OBJECT_DIR)/debuginfo.gen.o: $(BINARY_DIR)/kernel-nortdebug $(SOURCE_DIR)/error/debuginfo.gen.sh
	@$(ECHO) "Building $@"
	bash $(SOURCE_DIR)/error/debuginfo.gen.sh $(BINARY_DIR)/kernel-nortdebug > $(OBJECT_DIR)/debuginfo.gen.c
	$(CC) $(CCFLAGS) -c -o $@ $(OBJECT_DIR)/debuginfo.gen.c
	$(ARCH)-someos-objcopy --rename-section .rodata=.rtdebug $@ $@
# ==

