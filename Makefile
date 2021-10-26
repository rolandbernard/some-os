
include ./config.mk

# == General
ECHO := echo
# ==

# == Qemu
DISK := $(BUILD_DIR)/hdd.dsk
QEMU := qemu-system-$(ARCH)

# Qemu system
QEMU_ARGS := -M virt -smp 4 -m 128M -s $(QEMU_ADD_ARGS)
QEMU_ARGS += -cpu rv64 -bios none -snapshot

# Qemu devices
QEMU_ARGS += -monitor telnet:0.0.0.0:5000,server,nowait
QEMU_ARGS += -display none -serial stdio
QEMU_ARGS += -drive if=none,format=raw,file=$(DISK),id=disk0
QEMU_ARGS += -device virtio-blk-device,scsi=off,drive=disk0
# ==

.PHONY: build kernel userspace toolchain clean qemu

build: kernel userspace toolchain

kernel: toolchain
	$(MAKE) -C $(KERNEL_DIR)

userspace: toolchain
	$(MAKE) -C $(USERSPACE_DIR)
	mkdir -p $(SYSROOT_DIR)/dev
	mkdir -p $(SYSROOT_DIR)/bin
	cp -r $(USERSPACE_DIR)/build/$(BUILD)/bin/* $(SYSROOT_DIR)/bin

toolchain: $(BUILD_DIR)/toolchain.flag

$(BUILD_DIR)/%.flag:
	$(MAKE) -C $(ROOT_DIR)/$*
	touch $@

$(DISK): userspace toolchain | $(MOUNT_DIR)/
	@$(ECHO) "Building $@"
	dd if=/dev/zero of=$@ bs=1M count=128 &> /dev/null
	mkfs.minix -3 $@
	sudo mount $@ $(MOUNT_DIR)
	cp -r $(SYSROOT_DIR)/* $(MOUNT_DIR)/
	sudo umount $(MOUNT_DIR)

qemu: kernel $(DISK)
	$(QEMU) $(QEMU_ARGS) -kernel $(KERNEL_DIR)/build/$(BUILD)/bin/kernel

clean:
	$(MAKE) -C $(KERNEL_DIR) clean
	$(MAKE) -C $(USERSPACE_DIR) clean
	$(MAKE) -C $(TOOLCHAIN_DIR) clean
	$(RM) -rf $(BUILD_DIR)
	@$(ECHO) "Cleaned build directory."

