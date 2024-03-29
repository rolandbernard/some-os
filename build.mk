
# Common configuration for building the kernel and userspace

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

# == Directories
SOURCE_DIR := $(BASE_DIR)/src
OBJECT_DIR := $(BUILD_DIR)/$(BUILD)/obj
BINARY_DIR := $(BUILD_DIR)/$(BUILD)/bin
# ==

# == Common Flags
WARNINGS := -Wall -Wextra -Wno-unused-parameter -Wcast-qual

CCFLAGS.debug   += -O0 -g -DDEBUG -Werror
LDFLAGS.debug   += -O0 -g
CCFLAGS.release += -O3 -g
LDFLAGS.release += -O3 -g

CCFLAGS += $(CCFLAGS.$(BUILD)) $(WARNINGS) -MMD -MP -I$(SOURCE_DIR)
CCFLAGS += $(foreach SWITCH, $(SWITCHES), -D$(shell echo $(SWITCH) | tr '[:lower:]' '[:upper:]'))
CCFLAGS += $(foreach SWITCH, $(filter-out $(SWITCHES), $(ALL_SWITCHES)), -DNO$(shell echo $(SWITCH) | tr '[:lower:]' '[:upper:]'))
LDFLAGS += $(LDFLAGS.$(BUILD)) -static
LDLIBS  += $(LDLIBS.$(BUILD))
# ==

# == Files
PATTERNS         := *.c *.S
$(foreach SWITCH, $(ALL_SWITCHES), \
	$(eval SWITCH_SOURCES.$(SWITCH) \
		= $(foreach PATTERN, $(PATTERNS), $(shell find $(SOURCE_DIR) -type f -path '$(SOURCE_DIR)*/$(SWITCH)/*' -name '$(PATTERN)'))))
$(foreach TARGET, $(TARGETS), \
	$(eval TARGET_SOURCES.$(TARGET) \
		= $(foreach PATTERN, $(PATTERNS), $(shell find $(SOURCE_DIR) -type f -path '$(SOURCE_DIR)*/$(TARGET)/*' -name '$(PATTERN)'))))
ALL_SOURCES      := $(foreach PATTERN, $(PATTERNS), $(shell find $(SOURCE_DIR) -type f -name '$(PATTERN)'))
SWITCH_SOURCES   := $(foreach SWITCH, $(ALL_SWITCHES), $(SWITCH_SOURCES.$(SWITCH)))
ENABLED_SOURCES  := $(filter-out $(SWITCH_SOURCES), $(ALL_SOURCES)) $(foreach SWITCH, $(SWITCHES), $(SWITCH_SOURCES.$(SWITCH)))
DISABLED_SOURCES := $(filter-out $(ENABLED_SOURCES), $(ALL_SOURCES))
TARGET_SOURCES   := $(foreach TARGET, $(TARGETS), $(TARGET_SOURCES.$(TARGET)))
COMMON_SOURCES   := $(filter-out $(TARGET_SOURCES), $(ENABLED_SOURCES))
$(foreach TARGET, $(TARGETS), \
	$(eval TARGET_SOURCES.$(TARGET) = $(filter-out $(DISABLED_SOURCES), $(TARGET_SOURCES.$(TARGET)))))
$(foreach TARGET, $(TARGETS), \
	$(eval TARGET_OBJECTS.$(TARGET) = $(patsubst $(SOURCE_DIR)/%, $(OBJECT_DIR)/%.o, $(TARGET_SOURCES.$(TARGET)))))
COMPDBS          := $(patsubst $(SOURCE_DIR)/%, $(OBJECT_DIR)/%.compdb, $(ALL_SOURCES))
ALL_OBJECTS      := $(patsubst $(SOURCE_DIR)/%, $(OBJECT_DIR)/%.o, $(ALL_SOURCES))
OBJECTS          := $(patsubst $(SOURCE_DIR)/%, $(OBJECT_DIR)/%.o, $(COMMON_SOURCES))
DEPENDENCIES     := $(ALL_OBJECTS:.o=.d)
BINARYS          := $(foreach TARGET, $(TARGETS), $(BINARY_DIR)/$(TARGET))
# ==

.PHONY: build clean

build: compile_commands.json $(TARGETS)
	@$(FINISHED)
	@$(ECHO) "Build successful."

$(TARGETS): $(BINARY_DIR)/$$@

$(BINARYS): $(BINARY_DIR)/%: $(OBJECTS) $$(TARGET_OBJECTS.$$*) $(LINK_SCRIPT) $(PREBUILD.$(BUILD)) | $$(dir $$@)
	@$(ECHO) "Building $@"
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS) $(PREBUILD.$(BUILD)) $(TARGET_OBJECTS.$*) $(LDLIBS) $(if $(LINK_SCRIPT), -T$(LINK_SCRIPT))
	@$(CHANGED)

$(OBJECT_DIR)/%.o: $(SOURCE_DIR)/% $(MAKEFILE_LIST) | $$(dir $$@)
	@$(ECHO) "Building $@"
	$(CC) $(CCFLAGS) -c -o $@ $<

$(OBJECT_DIR)/%.compdb: $(SOURCE_DIR)/% | $$(dir $$@)
	@echo "    {" > $@
	@echo "        \"command\": \"cc  $(CCFLAGS) $(CCJFLAGS) -c $<\"," >> $@
	@echo "        \"directory\": \"$(BASE_DIR)\"," >> $@
	@echo "        \"file\": \"$<\"" >> $@
	@echo "    }," >> $@

compile_commands.json: $(COMPDBS)
	@echo "[" > $@.tmp
	@cat $^ >> $@.tmp
	@sed '$$d' < $@.tmp > $@
	@echo "    }" >> $@
	@echo "]" >> $@
	@rm $@.tmp

clean:
	$(RM) -rf $(BUILD_DIR)
	@$(ECHO) "Cleaned build directory."

-include $(DEPENDENCIES)

