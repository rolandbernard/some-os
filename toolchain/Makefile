
# == Directories
BUILD_DIR  := build
DISK_DIR   := ../disk
# ==

# == Flags
AC_ARGS := --prefix=/ --target=riscv64-someos
# ==

# == Progress
ECHO := echo
# ==

.SILENT:
.SECONDARY:
.SECONDEXPANSION:
.PHONY: newlib

newlib: | $(DISK_DIR)/ $(BUILD_DIR)/newlib/
	@$(ECHO) "Building $@"
	cd $(BUILD_DIR)/newlib && $(abspath newlib)/configure $(AC_ARGS)
	$(MAKE) -C $(BUILD_DIR)/newlib
	$(MAKE) -C $(BUILD_DIR)/newlib DESTDIR=$(abspath $(DISK_DIR)) install
	cp -ar $(DISK_DIR)/riscv64-someos/* $(DISK_DIR)/
	rm -rf $(DISK_DIR)/riscv64-someos

%/:
	@$(ECHO) "Building $@"
	mkdir -p $@

