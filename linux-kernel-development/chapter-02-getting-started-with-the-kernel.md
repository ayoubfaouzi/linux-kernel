# Getting Started with the Kernel

## Obtaining the Kernel Source

- The official and always up-to-date source is available at `kernel.org`, in two forms:
  - A **full tarball** of the kernel source.
  - An **incremental patch** to move from one version to the next.

### Using Git

- Kernel developers use **Git** for kernel development. It’s fast and designed for large, rapidly changing codebases.
- To clone the official kernel source:
  ```sh
  git clone git://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux-2.6.git
  ```

### Installing the Kernel Source from Tarballs

- Kernel tarballs come in two compression formats:
  - **bzip2** (`*.tar.bz2`) — preferred: smaller filesize.
  - **gzip** (`*.tar.gz`).
- To extract:
  - **bzip2:**: `tar xvjf linux-x.y.z.tar.bz2`
  - **gzip:**: `tar xvzf linux-x.y.z.tar.gz`
- This unpacks the source into `linux-x.y.z/`.
- If you're using Git, you don't need these tarballs.

### Where to Keep the Source

- Traditionally, some systems place the kernel source at `/usr/src/linux`, but:
- **Do NOT develop there.**
  - It's often tied to the C library and should not be modified.
- **Do NOT require root for development.**
  - Keep your development copy in your **home directory**, and only use `root` to install compiled kernels.

### Using Patches

- Patches are the primary format for sharing changes in the kernel community. All contributions, updates, and fixes are exchanged as patches.
- 👍:
  - Efficient **bandwidth** usage.
  - Easy **incremental** updates between versions.
  - Clear **visibility** of exactly what **changed**.
- To apply an incremental patch from within the kernel source directory: `patch -p1 < ../patch-x.y.z`
- Patches are usually applied against the **previous kernel version**.

## The Kernel Source Tree

### Overview of the Linux Kernel Source Tree

The Linux kernel source is organized into a structured hierarchy of directories, each responsible for a specific subsystem or architecture. At the top level (the root of the source tree), you’ll find the main directories listed below.

| Directory          | Description                                                             |
| ------------------ | ----------------------------------------------------------------------- |
| **arch/**          | Architecture-specific code (x86, ARM, RISC-V, etc.)                     |
| **block/**         | Block I/O layer and related infrastructure                              |
| **crypto/**        | Kernel Crypto API implementations                                       |
| **Documentation/** | Kernel documentation and subsystem guides                               |
| **drivers/**       | All hardware device drivers                                             |
| **firmware/**      | Firmware blobs required by certain drivers                              |
| **fs/**            | The Virtual Filesystem Switch (VFS) and filesystems (ext4, btrfs, etc.) |
| **include/**       | Global kernel header files                                              |
| **init/**          | Kernel boot and initialization code                                     |
| **ipc/**           | System V IPC subsystem (message queues, semaphores, shared memory)      |
| **kernel/**        | Core kernel components (scheduler, locking, timer system, etc.)         |
| **lib/**           | Generic helper and utility routines                                     |
| **mm/**            | Memory management and the virtual memory subsystem                      |
| **net/**           | Networking stack (TCP/IP, sockets, routing, etc.)                       |
| **samples/**       | Example code demonstrating kernel APIs                                  |
| **scripts/**       | Scripts used during kernel building and configuration                   |
| **security/**      | Linux Security Module (LSM) framework and modules (SELinux, AppArmor…)  |
| **sound/**         | Sound subsystem (ALSA, drivers)                                         |
| **usr/**           | Early userspace, including `initramfs` generation                       |
| **tools/**         | Developer tools (perf, bpftool, etc.)                                   |
| **virt/**          | Virtualization support (KVM, hypervisor interfaces)                     |

- Important Files in the Root Directory
  - **Makefile** — The top-level kernel Makefile controlling the build system.
- ▶️ These components form the foundation of the Linux kernel's modular and maintainable architecture.

## Building the Kernel

### Building the Linux Kernel

Building the kernel is straightforward, especially with the improved configuration and build system introduced in the 2.6 series. The process revolves around configuring the kernel, compiling it, and installing it.

### Configuring the Kernel

- Kernel configuration lets you customize features, subsystems, and drivers. Every option appears as a `CONFIG_FEATURE` entry in the `.config` file. These options determine:
  - What files get built
  - What code paths get compiled
  - What is built-in vs. modular
- Types of configuration options:
  - **Boolean (`yes`/`no`)**, examples: `CONFIG_PREEMPT`, `CONFIG_SMP`
  - **Tristate (`yes`/`module`/`no`)**: Used mostly for drivers; `module` builds them as dynamically loadable `.ko` modules.
  - **Strings/Integers**: Provide specific values used by the kernel (e.g., buffer sizes, hardware IDs).
- Common configuration methods:
  - **Interactive text-based**: `make config`
  - **Ncurses graphical menu** (recommended for terminals): `make menuconfig`
  - **GTK+ graphical UI**: `make gconfig`
- To generate **default** config for your architecture: `make defconfig`
- **Working With the `.config` File**
  - Stored at the **root** of the kernel source tree.
  - Many developers edit it **directly**, then validate it: `make oldconfig`
- If your kernel was built with `CONFIG_IKCONFIG_PROC=y`, you can extract it:

```bash
zcat /proc/config.gz > .config
make oldconfig
```

- Once configuration is set, to **build the kernel**: `make`.
- No need for `make dep` or separate module builds—the 2.6+ system handles everything.

### Minimizing Build Noise

- To hide normal compile spam: `make > /dev/null`
- Warnings/errors still appear because they go to _stderr_.

### Spawning Multiple Build Jobs

- The kernel’s Makefiles support reliable parallel builds: `make -jN`
  - Use **1–2 jobs per CPU core**.
  - Example for a 16-core machine: `make -j32 > /dev/null`
- Tools like **distcc** and **ccache** can speed things up even further.

### Installing the New Kernel

- Installation depends on your architecture and bootloader.
- General Steps
  - **Copy kernel image** to `/boot`
    - Example (x86 + GRUB):
      - Kernel image location: `arch/x86/boot/bzImage`
      - Copy to /boot: `cp arch/x86/boot/bzImage /boot/vmlinuz-version`
    - **Update bootloader**
      - **GRUB**: edit `/boot/grub/grub.conf`
      - **LILO**: edit `/etc/lilo.conf` then run: `lilo`
- ⚠️ Always keep at least one known-working kernel installed.
- **Modules** are installed automatically: `make modules_install`
  - This places them under: `/lib/modules/<kernel-version>/`
- The build process also generates `System.map`, a symbol-to-address lookup table used by **debugging** tools to map memory addresses to kernel functions/variables.

## A Beast of a Different Nature

- Developing kernel code differs significantly from writing user-space applications. These differences don’t make kernel development harder, but they _do_ make it **very different**, with unique rules and constraints. The most important distinctions are summarized below.

### No Standard C Library or System Headers

- The kernel is **not linked** against `libc` or any other external library.
- Reason: speed, size, and independence (kernel must boot without relying on user-space).
- Many libc-like utilities exist inside the kernel (e.g., string functions in `lib/string.c`).
- Kernel files must only include kernel headers, located under:
  - `include/`
  - `arch/<architecture>/include/asm/`
- Kernel uses `printk()` for logging because standard I/O is not available.
- Similar formatting to `printf()`.
- Uses priority flags such as `KERN_ERR` to categorize log messages.

### GNU C

- The Linux kernel is written in C, but _not_ strict ANSI C. It relies on:
  - **ISO C99 features**
  - **GNU C extensions**
- This ties the kernel to **gcc** (and compatible compilers like Intel’s icc).
- Important GNU C Features Used in the Kernel:
  - **Inline Functions**
    - Declared with `static inline`.
    - Used for small, performance-critical helpers.
    - Preferable to macros (better type safety and readability).
  - **Inline Assembly**
    - Used sparingly for architecture-specific operations.
    - Allows embedding raw assembly in C.
  - **Branch Annotation**
    - `likely()` and `unlikely()` wrap gcc intrinsics to optimize branch layout.
    - Commonly used to mark error paths as `unlikely()`.

### No Memory Protection

- User-space crashes → kernel sends **SIGSEGV**, process dies.
- Kernel crashes → **oops** (serious kernel fault) 🤷‍♀️.
- Invalid memory access or null dereference is catastrophic.
- Kernel memory is **not pageable** → anything allocated consumes real RAM.

### No Easy Floating-Point Operations

- User-space floating point is handled automatically by the kernel.
- The kernel cannot trap itself and thus cannot easily manage the FP unit.
- Using floating point inside the kernel requires manual saving/restoring FP state.
- ⚠️ **General rule: Do not use floating point in the kernel.**

### Small, Fixed-Size Stack

- Unlike user-space stacks, kernel stacks are:
  - **Small**
  - **Fixed in size**
  - **Per-process**
- Typical sizes:
  - 32-bit: 8KB
  - 64-bit: 16KB (historically two pages)
- This means:
  - Avoid large stack allocations.
  - Avoid deep recursion.
  - Prefer dynamic allocation (`kmalloc`, `kzalloc`).

### Concurrency and Synchronization Are Critical

- Several factors create concurrency inside the kernel:
  - **Preemptive scheduling**
  - **Symmetric multiprocessing (SMP)** — multiple CPUs running kernel code at once
  - **Asynchronous interrupts**
  - **Kernel preemption even inside kernel mode**
- These can all cause **race conditions** if shared data isn’t protected.
- Common synchronization primitives: Spinlocks, Semaphores, ...

### Importance of Portability

- Linux runs on many architectures, so kernel code must be portable:
  - Must work with different endianness.
  - Must be 32-bit and 64-bit clean.
  - Cannot assume page size, word size, or alignment rules.
  - Architecture-specific code lives under `arch/`.
- Portability keeps Linux running on everything from tiny embedded devices to large servers.
