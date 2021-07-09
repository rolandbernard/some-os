
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
# ==

# == Tools
CC       := clang -target $(ARCH)
LINK     := ld.lld
#==

# == Qemu
QEMU      := qemu-system-riscv64

QEMU_ARGS := -M virt -smp 4 -m 128M
QEMU_ARGS += -cpu rv64 -bios none
QEMU_ARGS += -display none -serial stdio
# ==

# == Flags
WARNINGS := -Wall -Wextra -Wno-unused-parameter

CCFLAGS.debug   += -O0 -g -DDEBUG
LDFLAGS.debug   += -O0 -g
CCFLAGS.release += -O3
LDFLAGS.release += -O3

CCFLAGS += $(CCFLAGS.$(BUILD)) $(WARNINGS) -MMD -MP -nostdlib -mno-relax -fpic
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
.PHONY: build clean

# Building

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

# Running

qemu: $(BINARY_DIR)/kernel
	$(QEMU) $(QEMU_ARGS) -kernel $(BINARY_DIR)/kernel

# Cleanup

clean:
	@$(ECHO) "Cleaning local files"
	$(RM) -rf $(BUILD_DIR)/*

-include $(DEPENDENCIES)

