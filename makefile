
# == Build type
BUILD := debug
# ==

# == Targets
TARGETS := kernel bootloader
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
		$(shell find $(SOURCE_DIR)/$(TARGET) -type f -name '*.c' 2> /dev/null) \
		$(shell find $(SOURCE_DIR)/ -type f -name '$(TARGET).c')) \
	$(eval OBJECTS.$(TARGET) := $(patsubst $(SOURCE_DIR)/%.c, $(OBJECT_DIR)/%.o, $(SOURCES.$(TARGET)))))
SOURCES        := $(shell find $(SOURCE_DIR) -type f -name '*.c')
ALL_OBJECTS    := $(patsubst $(SOURCE_DIR)/%.c, $(OBJECT_DIR)/%.o, $(SOURCES))
TARGET_OBJECTS := $(foreach TARGET, $(TARGETS), $(OBJECTS.$(TARGET)))
OBJECTS        := $(filter-out $(TARGET_OBJECTS), $(ALL_OBJECTS))
DEPENDENCIES   := $(ALL_OBJECTS:.o=.d)
BINARYS        := $(patsubst %, $(BINARY_DIR)/%, $(TARGETS))
# ==

# == Tools
CC       ?= clang
LINK     := $(CC)
OBJDUMP  ?= objdump
# ==

# == Flags
WARNINGS := -Wall -Wextra -Wno-unused-parameter

CCFLAGS.debug   += -O0 -g -DDEBUG
LDFLAGS.debug   += -O0 -g
CCFLAGS.release += -O3
LDFLAGS.release += -O3

CCFLAGS += $(CCFLAGS.$(BUILD)) $(WARNINGS) -MMD -MP -I$(SOURCE_DIR)
LDFLAGS += $(LDFLAGS.$(BUILD)) -static
LIBS    += -lm -lpthread -ldl
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

build: $(BINARYS)
	@$(ECHO) "Build successful."

$(BINARYS): $(BINARY_DIR)/%: $(OBJECTS) $$(OBJECTS.$$*) | $$(dir $$@)
	@$(ECHO) "Building $@"
	$(LINK) $(LDFLAGS) -o $@ $^ $(LIBS)

$(OBJECT_DIR)/%.o: $(SOURCE_DIR)/%.c $(MAKEFILE_LIST) | $$(dir $$@)
	@$(ECHO) "Building $@"
	$(CC) $(CCFLAGS) -c -o $@ $<

%/:
	@$(ECHO) "Building $@"
	mkdir -p $@

clean:
	@$(ECHO) "Cleaning local files"
	$(RM) -rf $(BUILD_DIR)/*

-include $(DEPENDENCIES)

