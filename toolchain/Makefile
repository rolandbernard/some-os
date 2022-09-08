
include ../config.mk

# == Config
TARGETS := gcc-no-headers binutils-no-headers newlib-no-headers binutils gcc newlib
TARGET  := $(ARCH)-someos
ECHO    := echo

ifeq ($(BUILD),debug)
export CFLAGS_FOR_TARGET := -mcmodel=medany -O0 -g
else
export CFLAGS_FOR_TARGET := -mcmodel=medany -O2
endif
# ==

# == gcc-no-headers
PROJ_NAME.gcc-no-headers := gcc
CONF_ARGS.gcc-no-headers := --prefix=$(TOOLS_DIR) --without-headers --enable-languages=c,c++
MAKE_ARGS.gcc-no-headers := all-gcc
INST_ARGS.gcc-no-headers := install-gcc
# ==

# == binutils-no-headers
PROJ_NAME.binutils-no-headers := binutils
CONF_ARGS.binutils-no-headers := --prefix=$(TOOLS_DIR) --without-headers --disable-werror
INST_ARGS.binutils-no-headers := install
# ==

# == newlib-no-headers
PROJ_NAME.newlib-no-headers  := newlib
BUILD_REQ.newlib-no-headers  := gcc-no-headers binutils-no-headers
CONF_ARGS.newlib-no-headers  := --prefix=/
INST_ARGS.newlib-no-headers  := DESTDIR=$(SYSROOT_DIR) install
POST_BUILD.newlib-no-headers := cp -ar $(SYSROOT_DIR)/$(TARGET)/* $(SYSROOT_DIR)/ && rm -rf $(SYSROOT_DIR)/$(TARGET)
# ==

# == gcc
PROJ_NAME.gcc := gcc
BUILD_REQ.gcc := newlib-no-headers
CONF_ARGS.gcc := --prefix=$(TOOLS_DIR) --with-sysroot=$(SYSROOT_DIR) --enable-languages=c,c++
MAKE_ARGS.gcc := all-gcc all-target-libgcc
INST_ARGS.gcc := install-gcc install-target-libgcc
# ==

# == binutils
PROJ_NAME.binutils := binutils
BUILD_REQ.binutils := newlib-no-headers
CONF_ARGS.binutils := --prefix=$(TOOLS_DIR) --with-sysroot=$(SYSROOT_DIR) --disable-werror
INST_ARGS.binutils := install
# ==

# == newlib
PROJ_NAME.newlib  := newlib
BUILD_REQ.newlib  := gcc binutils
CONF_ARGS.newlib  := --prefix=/
INST_ARGS.newlib  := DESTDIR=$(SYSROOT_DIR) install
POST_BUILD.newlib := cp -ar $(SYSROOT_DIR)/$(TARGET)/* $(SYSROOT_DIR)/ && rm -rf $(SYSROOT_DIR)/$(TARGET)
# ==

.PHONY: $(TARGETS)

# Assume we always changed
all: $(TARGETS)
	@$(CHANGED)

$(TARGETS): $$(BUILD_REQ.$$@) | $(SYSROOT_DIR)/ $(TOOLS_DIR)/ $(BUILD_DIR)/$$@/
	@$(ECHO) "Building $@"
	cd $(BUILD_DIR)/$@ && $(BASE_DIR)/$(PROJ_NAME.$@)/configure --target=$(TARGET) $(CONF_ARGS.$@)
	$(MAKE) -C $(BUILD_DIR)/$@ $(MAKE_ARGS.$@)
	$(MAKE) -C $(BUILD_DIR)/$@ $(INST_ARGS.$@)
	true ; $(POST_BUILD.$@)

$(foreach TARGET, $(TARGETS), only-$(TARGET)): only-%: | $(SYSROOT_DIR)/ $(TOOLS_DIR)/ $(BUILD_DIR)/$$*/
	@$(ECHO) "Building $*"
	cd $(BUILD_DIR)/$* && $(BASE_DIR)/$(PROJ_NAME.$*)/configure --target=$(TARGET) $(CONF_ARGS.$*)
	$(MAKE) -C $(BUILD_DIR)/$* $(MAKE_ARGS.$*)
	$(MAKE) -C $(BUILD_DIR)/$* $(INST_ARGS.$*)
	true ; $(POST_BUILD.$*)

clean:
	$(RM) -rf $(BUILD_DIR)
	@$(ECHO) "Cleaned build directory."

