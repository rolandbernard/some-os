
# == Build type
BUILD ?= debug
ARCH  ?= riscv64
BOARD ?= virt

export BUILD
export ARCH
export BOARD
# ==

# == Directories
ROOT_DIR      ?= $(shell git rev-parse --show-toplevel)
KERNEL_DIR    ?= $(ROOT_DIR)/kernel
USERSPACE_DIR ?= $(ROOT_DIR)/userspace
TOOLCHAIN_DIR ?= $(ROOT_DIR)/toolchain
PROGRAMS_DIR  ?= $(ROOT_DIR)/programs
SYSROOT_DIR   ?= $(ROOT_DIR)/build/sysroot
TOOLS_DIR     ?= $(ROOT_DIR)/build/tools
MOUNT_DIR     ?= $(ROOT_DIR)/build/mnt
BASE_DIR      ?= $(abspath .)
BUILD_DIR     ?= $(BASE_DIR)/build
# ==

# == Changed
CHANGED  = touch $(FLAG)
FINISHED = true
# ==

# == Common config
export PATH := $(TOOLS_DIR)/bin:$(PATH)

.SILENT:
.SECONDEXPANSION:
# ==

# == Common rules

%/:
	mkdir -p $@

