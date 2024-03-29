
include ../config.mk

# == Config
TARGETS := bash
TARGET  := $(ARCH)-someos
ECHO    := echo

ifeq ($(BUILD),debug)
export CFLAGS := -O0 -g -DNEED_EXTERN_PC
else
export CFLAGS := -O2 -g -DNEED_EXTERN_PC
endif
# ==

# == bash
PROJ_NAME.bash := bash
CONF_ARGS.bash := --host=$(TARGET) --prefix=/
MAKE_ARGS.bash := DESTDIR=$(SYSROOT_DIR)
INST_ARGS.bash := DESTDIR=$(SYSROOT_DIR) install
# ==

.PHONY: $(TARGETS)

# Assume we always changed
all: $(TARGETS)
	@$(CHANGED)

$(TARGETS): $$(BUILD_REQ.$$@) | $(SYSROOT_DIR)/ $(TOOLS_DIR)/ $(BUILD_DIR)/$$@/
	@$(ECHO) "Building $@"
	cd $(BUILD_DIR)/$@ && $(BASE_DIR)/$(PROJ_NAME.$@)/configure $(CONF_ARGS.$@)
	$(MAKE) -C $(BUILD_DIR)/$@ $(MAKE_ARGS.$@)
	$(MAKE) -C $(BUILD_DIR)/$@ $(INST_ARGS.$@)
	true ; $(POST_BUILD.$@)

$(foreach TARGET, $(TARGETS), only-$(TARGET)): only-%: | $(SYSROOT_DIR)/ $(TOOLS_DIR)/ $(BUILD_DIR)/$$*/
	@$(ECHO) "Building $*"
	cd $(BUILD_DIR)/$* && $(BASE_DIR)/$(PROJ_NAME.$*)/configure $(CONF_ARGS.$*)
	$(MAKE) -C $(BUILD_DIR)/$* $(MAKE_ARGS.$*)
	$(MAKE) -C $(BUILD_DIR)/$* $(INST_ARGS.$*)
	true ; $(POST_BUILD.$*)

clean:
	$(RM) -rf $(BUILD_DIR)
	@$(ECHO) "Cleaned build directory."

