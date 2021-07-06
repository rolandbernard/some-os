
# == Build type
BUILD := debug
ARCH  := amd64
# ==

# == Targets
TARGETS := kernel
# ==

# == Directories
SOURCE_DIR := src
BUILD_DIR  := build
OBJECT_DIR := $(BUILD_DIR)/$(BUILD)/obj
BINARY_DIR := $(BUILD_DIR)/$(BUILD)/bin
# ==

# == Files
$(foreach TARGET, $(TARGETS), \
	$(eval SOURCES.$(TARGET) := \
		$(shell find $(SOURCE_DIR)/$(TARGET)/$(ARCH) -type f -name '*.[cS]' 2> /dev/null) \
		$(shell find $(SOURCE_DIR)/$(TARGET) -maxdepth 1 -type f -name '*.[cS]' 2> /dev/null)) \
	$(eval OBJECTS.$(TARGET) := $(patsubst $(SOURCE_DIR)/%, $(OBJECT_DIR)/%.o, $(SOURCES.$(TARGET)))))
SOURCES        := $(shell find $(SOURCE_DIR) -type f -name '*.[cS]')
ALL_OBJECTS    := $(patsubst $(SOURCE_DIR)/%, $(OBJECT_DIR)/%.o, $(SOURCES))
TARGET_OBJECTS := $(foreach TARGET, $(TARGETS), $(OBJECTS.$(TARGET)))
OBJECTS        := $(filter-out $(TARGET_OBJECTS), $(ALL_OBJECTS))
DEPENDENCIES   := $(ALL_OBJECTS:.o=.d)
BINARYS        := $(patsubst %, $(BINARY_DIR)/%, $(TARGETS))
# ==

# == Tools
CC       ?= clang
LINK     ?= ld
OBJDUMP  ?= objdump

QEMU_BIN       = qemu-system-x86_64
QEMU_ARGS      = -smp 4 -m 128M -display none
QEMU_DEVICES   = -serial stdio
# ==

# == Flags
WARNINGS := -Wall -Wextra -Wno-unused-parameter

CCFLAGS.debug   += -O0 -g -DDEBUG
LDFLAGS.debug   += -O0 -g
CCFLAGS.release += -O3
LDFLAGS.release += -O3

CCFLAGS += $(CCFLAGS.$(BUILD)) $(WARNINGS) -MMD -MP -I$(SOURCE_DIR) -nostdlib 
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

$(BINARYS): $(BINARY_DIR)/%: $(OBJECTS) $$(OBJECTS.$$*) $(SOURCE_DIR)/$$*/$(ARCH)/link.ld | $$(dir $$@)
	@$(ECHO) "Building $@"
	$(LINK) $(LDFLAGS) -o $@  $(OBJECTS) $(OBJECTS.$*) -T$(SOURCE_DIR)/$*/$(ARCH)/link.ld

$(OBJECT_DIR)/%.c.o: $(SOURCE_DIR)/%.c $(MAKEFILE_LIST) | $$(dir $$@)
	@$(ECHO) "Building $@"
	$(CC) $(CCFLAGS) -c -o $@ $<

$(OBJECT_DIR)/%.S.o: $(SOURCE_DIR)/%.S $(MAKEFILE_LIST) | $$(dir $$@)
	@$(ECHO) "Building $@"
	$(CC) $(CCFLAGS) -c -o $@ $<

%/:
	@$(ECHO) "Building $@"
	mkdir -p $@

# Running

qemu: $(BINARY_DIR)/kernel
	$(QEMU_BIN) $(QEMU_ARGS) $(QEMU_DEVICES) -kernel $(BINARY_DIR)/kernel

# Cleanup

clean:
	@$(ECHO) "Cleaning local files"
	$(RM) -rf $(BUILD_DIR)/*

-include $(DEPENDENCIES)

