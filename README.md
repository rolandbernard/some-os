
Some OS
=======

This is just a simple operating system for RISC-V rv64gc targets. It is not intended to actually be
used.

## Development

### Requirements

The build will compile a version of `gcc` and `binutils` for the `riscv64-someos` target, if your
system is able to compile both of them you should be fine.

To run the kernel in a virtual machine you will need to have [QEMU](https://www.qemu.org/) with
riscv64 support installed.

### Getting the source

Because this repository uses submodules to include all ported software, you will have to recursively
clone this repository. You can do this using the following command:

```
git clone --recursive https://github.com/rolandbernard/some-os
```

Or if you have already cloned the repository:

```
git submodule update --init --recursive
```

### Building

Compile the project using the `make` command. This will build the toolchain, the kernel and all
userspace components. This command can take quite some time.
You might also want to build in release mode using `make BUILD=release`.

### Running

To start the operating system in QEMU run `make qemu`. This will also build a minix3 formatted disk
in `build/hdd.dsk` from the `sysroot` directory. To do this your system needs the ability to format
a file using `mkfs.minix` and mount the created file using `mount` and unmount using `umount`.

### Repository structure

This repository has four important subdirectories:

* `kernel` This directory includes all the code for the kernel
* `userspace` This directory includes some custom userspace programs
* `toolchain` This directory includes ported software that belongs to the toolchain
* `programs` This directory includes all ported programs that don't belong to the toolchain

