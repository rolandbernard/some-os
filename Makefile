
include ./config.mk

# == General
ECHO := echo
SUBS := toolchain kernel userspace

$(foreach SUB, $(SUBS), $(eval TARGET.$(SUB) := $(BUILD_DIR)/$(SUB).flag))
TARGET.sysroot := $(BUILD_DIR)/sysroot.flag
# ==

# == Qemu
DISK    := $(BUILD_DIR)/hdd.dsk
KERNEL  := $(KERNEL_DIR)/build/$(BUILD)/bin/kernel
QEMU    := qemu-system-$(ARCH)
GDB     := $(ARCH)-someos-gdb

# Qemu system
QEMU_SMP  ?= 1
QEMU_ARGS := -M virt -m 128M -s $(QEMU_ADD_ARGS)
QEMU_ARGS += -cpu rv64 -bios none -snapshot

# Qemu devices
QEMU_ARGS += -monitor telnet:0.0.0.0:5000,server,nowait
QEMU_ARGS += -display none -serial stdio
QEMU_ARGS += -drive if=none,format=raw,file=$(DISK),id=disk0
QEMU_ARGS += -device virtio-blk-device,scsi=off,drive=disk0
# ==

.PHONY: build clean qemu $(SUBS)

build: $(TARGET.kernel) $(TARGET.userspace)

$(TARGET.userspace): $(TARGET.toolchain) FORCE
$(TARGET.kernel): $(TARGET.toolchain) FORCE

$(BUILD_DIR)/%.flag:
	@$(ECHO) "Building $*"
	$(MAKE) -C $(ROOT_DIR)/$* FLAG=$(BUILD_DIR)/$*.flag

$(TARGET.sysroot): $(TARGET.userspace) $(MAKEFILE_LIST)
	@$(ECHO) "Building sysroot"
	mkdir -p $(SYSROOT_DIR)/dev
	mkdir -p $(SYSROOT_DIR)/bin
	cp -r $(USERSPACE_DIR)/build/$(BUILD)/bin/* $(SYSROOT_DIR)/bin
	touch $@

$(DISK): $(TARGET.sysroot) | $(MOUNT_DIR)/
	@$(ECHO) "Building $@"
	dd if=/dev/zero of=$@ bs=1M count=128 &> /dev/null
	mkfs.minix -3 $@
	sudo mount $@ $(MOUNT_DIR)
	cp -r $(SYSROOT_DIR)/* $(MOUNT_DIR)/
	sudo umount $(MOUNT_DIR)

qemu: $(TARGET.kernel) $(DISK)
	$(QEMU) $(QEMU_ARGS) -smp $(QEMU_SMP) -kernel $(KERNEL)

qemu-gdb: $(TARGET.kernel) $(DISK)
	$(QEMU) $(QEMU_ARGS) -smp 1 -S -kernel $(KERNEL)

gdb:
	$(GDB) --init-eval-command="target remote localhost:1234" $(KERNEL)

sysroot: $(TARGET.sysroot)

$(SUBS): $$(TARGET.$$@)

$(foreach SUB, $(SUBS), only-$(SUB)): only-%:
	$(MAKE) -C $(ROOT_DIR)/$*

$(foreach SUB, $(SUBS), clean-$(SUB)): clean-%:
	$(MAKE) -C $(ROOT_DIR)/$* clean

clean:
	$(MAKE) -C $(KERNEL_DIR) clean
	$(MAKE) -C $(USERSPACE_DIR) clean
	$(MAKE) -C $(TOOLCHAIN_DIR) clean
	$(RM) -rf $(BUILD_DIR)
	@$(ECHO) "Cleaned build directory."

FORCE:

