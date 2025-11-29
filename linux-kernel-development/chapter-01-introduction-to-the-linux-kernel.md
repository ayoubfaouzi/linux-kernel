# Introduction to the Linux Kernel

## History of Unix

- Birth of Unix (1969–1973)
  - Unix was created in **1969** by **Dennis Ritchie** and **Ken Thompson** at Bell Labs after the failure of the Multics project.
  - Started as a filesystem experiment implemented on a **PDP-7**.
  - Ported to the **PDP-11** in 1971.
  - In **1973**, it was **rewritten in C**, making it remarkably portable — a revolutionary idea at the time.
  - The first widely used version was **Unix Version 6 (V6)**.
- Commercial and Academic Expansion:
  - Companies began porting Unix to new hardware, creating many variants.
  - Bell Labs consolidated variants into **System III (1977)** and later **System V (1982)**.
- **University of California at Berkeley** became the most influential outside contributor.
  - Their releases became known as **BSD (Berkeley Software Distribution)**.
- **BSD Lineage (1977–1994)**:
  - **1BSD (1977)**: patches and utilities on top of Bell Labs’ Unix.
  - **2BSD (1978)**: added `csh` and `vi`.
  - **3BSD (1979)**: first standalone BSD, introduced **virtual memory**.
  - **4BSD series** (4.0, 4.1, 4.2, 4.3):
    - Major additions such as **job control**, **demand paging**, and **TCP/IP**.
  - **4.4BSD (1994)**: final Berkeley release, with a rewritten VM system.
- Modern descendants include **FreeBSD**, **NetBSD**, **OpenBSD**, and **Darwin** (macOS foundation) 🤷‍♀️.
- **Commercial Unix Systems (1980s–1990s)**:
  - Different companies produced their own proprietary Unix variants, generally based on AT&T or BSD branches:
    - **Tru64 (Digital)**
    - **HP-UX (Hewlett Packard)**
    - **AIX (IBM)**
    - **DYNIX/ptx (Sequent)**
    - **IRIX (SGI)**
    - **Solaris/SunOS (Sun Microsystems)**
- ➡️ These systems focused on features optimized for their hardware architectures.
- **Why Unix Endured ❓**:
  - Unix’s longevity and influence stem from a few essential characteristics:
  - **Simplicity**: A small set of system calls and a clear design philosophy.
  - **“Everything is a file”**:
    - Devices, pipes, sockets—almost everything is represented like a file.
    - Allows uniform interaction via the core operations: `open()`, `read()`, `write()`, `lseek()`, `close()`.
  - **Portability**: Written in **C**, enabling easy porting to new hardware.
  - **Efficient Process Model**: Fast process creation and the powerful **fork()** system call.
  - **Clean IPC and Composability**
    - Simple, reliable primitives.
    - Encourages the creation of small programs that do one thing well and can be combined into complex workflows.
- Modern Unix variants support:
  - **Preemptive multitasking**
  - **Multithreading**
  - **Virtual memory & demand paging**
  - **Shared libraries**
  - **TCP/IP networking**
  - Some scale to **hundreds of processors**, others run on **embedded devices** 💖.
- Unix remains powerful because:
  - Its original design was **simple, clean, and extensible**.
  - Early architectural decisions allowed Unix to **evolve for decades** without losing coherence or elegance.

## Along Came Linus: Introduction to Linux

- In **1991**, **Linus Torvalds** began developing Linux as a response to the lack of a free, powerful Unix-like system for the **Intel 80386**.
- Existing options frustrated him:
  - **MS-DOS** was too limited for serious work (useful only to play _Prince Of Persia_ 😆).
  - **Minix**, while Unix-like, had **licensing restrictions** and design choices that hindered modification and distribution.
- He released an early version publicly on the Internet in **late 1991**, and adoption quickly grew.
  - Its **free license** encouraged contributions, modifications, and redistributions.
  - Early Linux distributions helped spread the system, but its rapid technical advancement came from a global community of hackers continually improving the code.
- Modern Linux runs on many architectures: **Alpha, ARM, PowerPC, SPARC, x86-64**, and more.
- It scales from **tiny embedded devices** (watches, routers) to **massive supercomputers** and large datacenters.
- Commercial interest is strong: companies like **Red Hat** and **IBM** provide Linux-based products across embedded, mobile, desktop, and server markets.
- **Linux Is Unix-like, Not Unix**:
  - Linux **implements Unix concepts** and follows the **POSIX** and **Single Unix Specification** APIs.
  - It is **not** a direct descendant of Bell Labs’ Unix source code.
  - Where appropriate, Linux diverges from traditional Unix implementations, but it remains true to Unix design principles: simplicity, portability, and clear interfaces 👍,
- **Open Source and the GPL**:
  - Linux is **not a commercial proprietary product**.
  - It is a **collaborative, internet-driven project**, maintained by Linus Torvalds and thousands of developers worldwide.
  - The kernel is licensed under the **GNU General Public License (GPL) v2**, which ensures:
    - You can freely obtain, modify, and redistribute the source.
    - Any distributed modifications must remain under the same license and include source code.
- **What "Linux" Actually Means**
  - Strictly speaking, **Linux = the kernel** only.
  - A complete “Linux system” typically includes:
    - The **kernel**
    - A **C library** (e.g., glibc)
    - A **toolchain** (compiler, assembler, linker)
    - Basic system utilities (shell, login, core tools)
    - Optionally: `X11`, `GNOME`, `KDE`, and many other applications.

## Overview of Operating Systems and Kernels

- In this book, the **operating system** refers only to the essential components required for system use and administration:
  - **Kernel**
  - **Device drivers**
  - **Boot loader**
  - **Shell or basic user interface**
  - **Core file and system utilities**
- Everything else (like browsers or 🎵 players) belongs to the larger **system**, meaning the OS _plus_ all user applications.
- The kernel is the **innermost and most fundamental** part of the operating system.
  - It provides basic services, manages hardware, and allocates system resources.
- **Typical kernel components include:**
  - **Interrupt handlers**
  - **Scheduler** (shares CPU time among processes)
  - **Memory management system** (manages address spaces)
  - **System services** such as networking and IPC
- Modern systems with memory protection enforce two execution domains:
  - **Kernel Space**
    - Reserved for kernel code.
    - Has full hardware access, protected memory, and elevated privileges.
    - Executes in _kernel mode_.
  - **User Space**
    - Where regular applications run.
    - Limited access to hardware and memory.
    - Executes in _user mode_.
- Switching between the two involves carefully controlled transitions, usually through **system calls** or interrupts.
- Applications cannot directly access kernel services; they call into the kernel via **system calls**.
- Typically, a program calls a function in a **library** (like _glibc_), and that function performs the actual system call.
  - `printf()` → formats text, buffers it, and eventually calls `write()`
  - `open()` (library) → mostly just invokes the `open()` system call
  - `strcpy()` → _shouldn’t_ enter the kernel at all ❓
- When a system call runs:
  - The kernel is **executing on behalf of the process**.
  - The process is said to be **in kernel space** but still running in **process context**.
- Hardware devices communicate by issuing **interrupts**.
- An interrupt:
  1. Stops the processor
  2. Triggers the kernel
  3. Calls an interrupt handler identified by the interrupt number
- Example:
  - When typing on a keyboard, it triggers a **keyboard interrupt**.
  - The handler reads the key data and signals the device that it's ready for more.
- **Interrupt handlers** run in a **special interrupt context**, not tied to any process.
  - They must do minimal, fast work.
  - The kernel may disable interrupts temporarily for synchronization.
- At any moment, each processor in a Linux system is in exactly **one** of these states:
  1. **User-space**, running user code
  2. **Kernel-space in process context**, running on behalf of a process
  3. **Kernel-space in interrupt context**, handling a hardware interrupt
- Even special cases fit one of these:
  - When a CPU is idle, it’s actually running the **idle process**, which is still process context inside the kernel.
  <p align="center"><img src="./assets/linux-arch.png" width="400px" height="auto"></p>

## Linux Versus Classic Unix Kernels

- Unix kernels share design traits due to **common ancestry** and adherence to the **Unix/POSIX API**.
- Classic Unix kernels are:
  - **Monolithic** (single, static binary image)
  - Running in **one address space**
  - Dependent on hardware with an **MMU** (for memory protection and per-process virtual address spaces)
- Linux historically required an MMU too, though minimal MMU-less variants exist for embedded systems.
- **Monolithic Kernels**
  - Entire kernel is **one large process** in a single address space.
  - All services run in **kernel mode** and communicate by simple **direct function calls**.
  - 👍: **Simplicity** && **High performance**.
  - Traditional Unix kernels (and Linux) follow this model.
- **Microkernels**
  - Split functionality into independent **servers** (processes).
  - Ideally, only essential components run in kernel mode; others run in user space.
  - Servers communicate by **IPC message passing** instead of function calls.
  - 👍: **Modular design** && **Fault isolation**.
  - 👎: Significant overhead: **message passing** costs && frequent user/kernel **context switches**.
- Practically, modern microkernels compromise:
  - `Windows NT` and `Mach` both moved most servers back into kernel space 🤷, weakening the original microkernel design.
- Linux is **monolithic**, but it incorporates many microkernel ideas without sacrificing performance:
  - **Modular design** (kernel modules can be loaded/unloaded dynamically)
  - **Kernel preemption** (the kernel can interrupt itself)
  - **Kernel threads**
  - **Schedulable kernel**
  - **Direct function calls** (no IPC overhead)
  - **Everything runs in one address space** (monolithic performance preserved)
- This blend combines monolithic speed with microkernel flexibility — a pragmatic design.
- Linux differs from traditional Unix systems in several important ways:
  - **Dynamic Kernel Modules**
    - Linux supports loading/unloading kernel modules at runtime, despite being monolithic.
  - **SMP (Symmetric Multiprocessing) Support**
    - Built-in and well-integrated SMP support.
    - Classic Unix systems didn’t always support SMP initially.
  - **Kernel Preemption**
    - Linux allows preemption even _within the kernel_.
    - Traditional Unix kernels are usually **non-preemptive**.
    - Commercial exceptions: `Solaris`, `IRIX`.
  - **Unified Process/Thread Model**
    - Linux does not distinguish between threads and processes internally.
    - All tasks are treated uniformly; threads are simply tasks sharing certain resources.
  - **Modern Device Model**
    - Object-oriented structure
    - Device classes
    - Hot-plug events
    - `sysfs` for exposing device information to user-space
  - **Selective Adoption of Unix Features**
    - Linux intentionally omits features considered poorly designed, such as **STREAMS**.
    - It avoids implementing standards that cannot be done cleanly.
  - **Open Development Philosophy**
    - Linux is completely free and openly developed.
    - Only clean, useful, well-designed features are accepted.
    - Features without solid technical value (e.g., pageable kernel memory used by some **commercial** Unixes) are rejected 💯.
- 👉 Despite the differences, Linux remains strongly **Unix-like**, adhering closely to Unix design goals and the standard Unix API, while enjoying the freedom to innovate where beneficial.

## Linux Kernel Versions

- Linux kernels **historically** came in two types:
  - **Stable kernels**
    - Production-ready
    - Receive only bug fixes and new drivers
    - Intended for widespread deployment
  - **Development kernels**
    - Rapidly changing
    - Used to test new ideas and large architectural changes
    - Can be unstable
- Traditionally, Linux kernel versions used a **three- or four-part number** (i.e **2.6.30.1**):
  - **Major**: 2 | **Minor**: 6 | **Revision**: 30 | **Stable update**: 1
  - **Even minor number → stable**: Example: `2.6.x`, `2.4.x`
  - **Odd minor number → development** : Example: `2.5.x`, `2.3.x`
  - So kernel **2.6.30.1** is stable, and **2.5.x** was development.
  - The first two numbers describe the **kernel series** (e.g., the _2.6 series_).
  - A development series (e.g., 2.5) went through phases:
    1. **Early phase**
       - Heavy experimentation
       - Big, destabilizing changes
    2. **Feature freeze**
       - No new features accepted
       - Work continues on existing ones
    3. **Code freeze**
       - Only bug fixes accepted
       - Stabilization phase
    4. **First stable release**
       - Example:
         - 1.3 → stabilized into → 2.0
         - 2.5 → stabilized into → 2.6
  - Within a stable series, Linus released revisions incrementally:
    - 2.6.0
    - 2.6.1
    - 2.6.2
    - … etc.
  - Differences between revisions were small and incremental.
- At the **2004** Kernel Developers Summit, major leadership (`Linus`, `Andrew Morton`, others) decided:
  - **The 2.6 series was stable and mature**
  - A new destabilizing 2.7 series was **not necessary**
  - Development could continue _within_ 2.6 using a new model
  - 👉 This approach became the modern release cycle.
- Instead of large unstable branches, each 2.6 revision contains:
  - A small development cycle
  - Followed by stabilization
  - Result:
    - **Longer release intervals** (several months)
    - **More features per release**
    - Still stable overall
- Andrew Morton’s **2.6-mm** tree became a testing ground for big or risky patches.
- Destabilizing changes flow through:
  1. **mm tree** → testing ground
  2. **mini-development cycle**
  3. **mainline release** (e.g., 2.6.29)
- 👉 This process was successful and continued for years, effectively eliminating the need for a separate development branch.
- Because regular releases slowed down, developers introduced: **Stable patch releases**
  - Example: **2.6.32.8** = 8th stable patch to 2.6.32
- These patches include:
  - Important bug fixes
  - Security fixes
  - Backported fixes from ongoing development (e.g., 2.6.33)
- 👉 This keeps older releases secure and stable while new ones are developed.

## The Linux Kernel Development Community

The Linux kernel is built by a **global, highly active, and highly skilled community**. When you start contributing, you effectively join this group.

- **Linux Kernel Mailing List (LKML)**
  - The **primary communication hub** for kernel developers.
  - Hosted at _vger.kernel.org_.
  - Known for **very high traffic** — hundreds of messages per day.
  - Includes **all major contributors**, including Linus Torvalds himself.
  - Participants expect:
    - Technical accuracy
    - No off-topic messages
    - No hand-holding
- Despite its intensity, LKML is extremely valuable 💡.
- **Why LKML Matters**
  - Discuss new ideas
  - Submit patches and receive review
  - Ask technical questions
  - Recruit testers for new work
- Reading LKML (even silently 🦊, “lurking”) is one of the best ways to learn how kernel development works in practice.
