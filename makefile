
# == Build type
BUILD := debug
ARCH  := riscv64
# ==

# == Targets
TARGETS := kernel
ARCHS   := riscv64
# ==

# == Directories
SOURCE_DIR := src
BUILD_DIR  := build
OBJECT_DIR := $(BUILD_DIR)/$(BUILD)/obj
BINARY_DIR := $(BUILD_DIR)/$(BUILD)/bin
MOUNT_DIR  := mnt
DISK_DIR   := disk
USERSPACE_DIR := userspace
USERSPACE_BIN_DIR := $(USERSPACE_DIR)/build/$(BUILD)/bin
# ==

# == Files
$(foreach ARC, $(ARCHS), \
	$(eval SOURCES.$(ARC) := \
		$(shell find $(SOURCE_DIR) -type f -path '*/$(ARC)/*.[cS]' 2> /dev/null)) \
	$(eval OBJECTS.$(ARC) := $(patsubst $(SOURCE_DIR)/%, $(OBJECT_DIR)/%.o, $(SOURCES.$(ARC)))))
ARCH_OBJECTS   := $(filter-out $(OBJECTS.$(ARCH)), $(foreach ARC, $(ARCHS), $(OBJECTS.$(ARC))))
$(foreach TARGET, $(TARGETS), \
	$(eval SOURCES.$(TARGET) := \
		$(shell find $(SOURCE_DIR)/$(TARGET) -type f -name '*.[cS]' 2> /dev/null)) \
	$(eval OBJECTS.$(TARGET) := $(filter-out $(ARCH_OBJECTS), $(patsubst $(SOURCE_DIR)/%, $(OBJECT_DIR)/%.o, $(SOURCES.$(TARGET))))))
SOURCES        := $(shell find $(SOURCE_DIR) -type f -name '*.[cS]')
ALL_OBJECTS    := $(patsubst $(SOURCE_DIR)/%, $(OBJECT_DIR)/%.o, $(SOURCES))
TARGET_OBJECTS := $(foreach TARGET, $(TARGETS), $(OBJECTS.$(TARGET)))
OBJECTS        := $(filter-out $(ARCH_OBJECTS) $(TARGET_OBJECTS), $(ALL_OBJECTS))
DEPENDENCIES   := $(ALL_OBJECTS:.o=.d)
BINARYS        := $(patsubst %, $(BINARY_DIR)/%, $(TARGETS))
DISK           := $(BUILD_DIR)/hdd.dsk
# ==

# == Tools
LINK := ld.lld
CC   := clang -target $(ARCH)
ifeq ($(ARCH),riscv64)
CC   += -march=rv64gc
endif
#==

# == Qemu
QEMU      := qemu-system-$(ARCH)

# Qemu system
QEMU_ARGS := -M virt -smp 4 -m 128M -s $(QEMU_ADD_ARGS)
QEMU_ARGS += -cpu rv64 -bios none -snapshot

# Qemu devices
QEMU_ARGS += -monitor telnet:0.0.0.0:5000,server,nowait
QEMU_ARGS += -display none -serial stdio
QEMU_ARGS += -drive if=none,format=raw,file=$(DISK),id=disk0
QEMU_ARGS += -device virtio-blk-device,scsi=off,drive=disk0
# ==

# == Flags
WARNINGS := -Wall -Wextra -Wno-unused-parameter

CCFLAGS.debug   += -O0 -g -DDEBUG
LDFLAGS.debug   += -O0 -g
CCFLAGS.release += -O3
LDFLAGS.release += -O3

CCFLAGS += $(CCFLAGS.$(BUILD)) $(WARNINGS) -MMD -MP -nostdlib -mno-relax -fpic -ffreestanding
CCFLAGS += -I$(SOURCE_DIR) -I$(SOURCE_DIR)/libc/include
LDFLAGS += $(LDFLAGS.$(BUILD)) -static
# ==

# == Progress
ifndef ECHO
TOTAL   := \
	$(shell $(MAKE) $(MAKECMDGOALS) --no-print-directory -nrRf $(firstword $(MAKEFILE_LIST)) \
		ECHO="__HIT_MARKER__" BUILD=$(BUILD) | grep -c "__HIT_MARKER__")
TLENGTH := $(shell expr length $(TOTAL))
COUNTER  = $(words $(HIDDEN_COUNT))
COUNTINC = $(eval HIDDEN_COUNT := x $(HIDDEN_COUNT))
PERCENT  = $(shell expr $(COUNTER) '*' 100 / $(TOTAL))
ECHO     = $(COUNTINC)printf "[%*i/%i](%3i%%) %s\n" $(TLENGTH) $(COUNTER) $(TOTAL) $(PERCENT)
endif
# ==

.SILENT:
.SECONDARY:
.SECONDEXPANSION:
.PHONY: build clean qemu force

# Building kernel

build: $(BINARYS)
	@$(ECHO) "Build successful."

$(BINARYS): $(BINARY_DIR)/%: $$(OBJECTS) $$(OBJECTS.$$*) $(SOURCE_DIR)/$$*/$(ARCH)/link.ld | $$(dir $$@)
	@$(ECHO) "Building $@"
	$(LINK) $(LDFLAGS) -o $@ $(OBJECTS) $(OBJECTS.$*) -T$(SOURCE_DIR)/$*/$(ARCH)/link.ld

$(OBJECT_DIR)/%.o: $(SOURCE_DIR)/% $(MAKEFILE_LIST) | $$(dir $$@)
	@$(ECHO) "Building $@"
	$(CC) $(CCFLAGS) -c -o $@ $<

%/:
	@$(ECHO) "Building $@"
	mkdir -p $@

# Building disk

force:

$(USERSPACE_BIN_DIR): force
	$(MAKE) $(MFLAGS) -C $(USERSPACE_DIR)

$(DISK_DIR): $(USERSPACE_BIN_DIR) $(MAKEFILE_LIST)
	@$(ECHO) "Creating disk files"
	mkdir -p $(DISK_DIR)
	rm -f -r $(DISK_DIR)/*
	mkdir $(DISK_DIR)/test
	mkdir $(DISK_DIR)/test2
	echo "Hello world!" > $(DISK_DIR)/test/test.txt
	cp -r $(USERSPACE_BIN_DIR) $(DISK_DIR)

$(DISK): $(DISK_DIR) $(MAKEFILE_LIST) | $(MOUNT_DIR)
	@$(ECHO) "Building $@"
	dd if=/dev/zero of=$@ bs=1M count=32 &> /dev/null
	mkfs.minix -3 $@
	sudo mount $@ $(MOUNT_DIR)
	cp -r $(DISK_DIR)/* $(MOUNT_DIR)/
	sudo umount $(MOUNT_DIR)

# Running

qemu: $(BINARY_DIR)/kernel $(DISK)
	$(QEMU) $(QEMU_ARGS) -kernel $(BINARY_DIR)/kernel

# Cleanup

clean:
	@$(ECHO) "Cleaning local files"
	$(RM) -rf $(BUILD_DIR)/*

-include $(DEPENDENCIES)

