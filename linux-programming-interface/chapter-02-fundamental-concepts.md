# Fundamental Concepts

## The Core Operating System: The Kernel

- The term **operating system** is used in two ways:
  - **Broad sense**: The entire software suite managing the computer, including the kernel plus tools such as shells, GUIs, editors, and utilities.
  - **Narrow sense**: The core software that manages hardware resources (CPU, memory, devices).
- The **kernel** corresponds to the second meaning. This book focuses on the kernel, because it is the component that directly manages system resources and provides essential services to programs.
- Although programs could theoretically run without a kernel, the kernel:
  - Simplifies program development
  - Coordinates access to limited resources
  - Enables powerful abstractions such as processes, files, and virtual memory
- The **Linux Kernel executable** is yypically located at: **`/boot/vmlinuz`**
- Name evolution:
  - `unix` → early UNIX kernels
  - `vmunix` → virtual memory–enabled kernels
  - `vmlinuz` → Linux kernel, with `z` indicating compression

### Tasks Performed by the Kernel

- **Process scheduling**: Linux is a **preemptive multitasking** OS
  - Multiple processes reside in memory simultaneously
  - The kernel scheduler decides: Which process runs, when it runs & for how long.
- **Memory management**:
  - Physical RAM is limited
  - Linux uses **virtual memory**, which:
    - Isolates processes from each other and the kernel
    - Allows partial process residency in RAM
    - Improves CPU utilization by keeping runnable processes available
- **File system management**
  - Provides a persistent storage abstraction
  - Supports creating, reading, updating, and deleting files
- **Process lifecycle management**
  - Loads programs into memory as processes
  - Allocates required resources
  - Reclaims resources when processes terminate
- **Device access**
  - Provides standardized interfaces to hardware
  - Arbitrates access to devices among competing processes
  - Device drivers perform actual I/O
- **Networking**
  - Sends and receives packets on behalf of processes
  - Handles routing and delivery
- **System call API**
  - Programs request kernel services via **system calls**
  - This API is the primary interface between user programs and the kernel
  - Linux provides each user with the illusion of a **private machine**:
  - Separate **home directories**
- **Independent** _processes_
  - Isolated virtual address spaces
  - **Shared hardware** resources transparently managed by the kernel
  - 🧠 The kernel resolves conflicts so users and processes are generally unaware of contention.

### Kernel Mode vs User Mode

- Modern CPUs support at least two execution modes:
- **User mode**
  - Restricted access
  - Cannot access kernel memory or privileged instructions
- **Kernel mode**
  - Full access to memory and hardware
  - Required for operations such as:
    - Device I/O
    - Memory management
    - System control instructions
- This separation protects system stability and security by preventing user programs from directly manipulating hardware or kernel internals.

### Process View vs Kernel View

- An executing process experiences asynchronous events (scheduling, signals, IPC)
- Is unaware of:
  - Physical memory locations
  - Swapping
  - Disk layout of files
- Cannot:
  - Directly access devices
  - Communicate with other processes directly
  - Create or terminate processes on its own
- From **Kernel perspective**:
  - Has global knowledge and control
  - Maintains:
    - Process tables
    - Memory mappings
    - File system metadata
  - Schedules CPU usage
  - Mediates all interprocess communication
  - Performs all hardware interaction via device drivers
- Statements like “a process creates another process” are shorthand for “the process requests that the kernel perform this action.” 🤷‍♀️

## The Shell

- A **shell** is a user-space program that:
  - Reads commands typed by a user
  - Interprets those commands
  - Executes the appropriate programs
- For this reason, a shell is often called a **command interpreter**.
- A **login shell** is the shell process that starts when a user first logs into the system.
- Unlike some operating systems where the command interpreter is part of the **kernel**:
  - In **UNIX/Linux**, the shell runs as a **normal user process**
  - Multiple shells can exist on the same system
  - Different users (or even one user) can use **different shells simultaneously**
  - This design reinforces UNIX’s philosophy of keeping the kernel small and pushing policy to user space 🧠.
- **Bourne Shell (sh)**:
  - Written by **Steve Bourne**
  - Standard shell for **Seventh Edition UNIX**
  - Introduced core shell features:
    - I/O redirection and pipelines
    - Filename globbing
    - Variables and environment handling
    - Command substitution
    - Background execution
    - Functions
  - Still present on all UNIX systems
  - Forms the foundation for most later shells
- **C Shell (csh)**
  - Written by **Bill Joy** at UC Berkeley
  - Syntax resembles the **C language**
  - Added powerful interactive features:
    - Command history
    - Command-line editing
    - Job control
    - Aliases
  - **Not backward compatible** with Bourne shell
  - Common as an interactive shell on BSD systems
  - Scripts were usually written in **sh** for portability
- **Korn Shell (ksh)**
  - Written by **David Korn** at AT&T Bell Labs
  - Designed as a **successor to Bourne shell**
  - Fully backward compatible with sh
  - Added interactive features similar to csh
  - Influenced POSIX shell standardization
- **Bourne Again Shell (bash)**
  - GNU project’s reimplementation of sh
  - Main authors: **Brian Fox** and **Chet Ramey**
  - Combines:
    - Bourne shell compatibility
    - Interactive features from csh and ksh
  - **Most widely used shell on Linux**
  - On Linux:
    - `/bin/sh` is typically **bash running in sh-compatibility mode**
- **POSIX.2 (1992)** standardized a shell interface
  - Based largely on the Korn shell
  - Both **bash** and **ksh** conform to POSIX
  - Each also provides non-standard extensions, which differ between shells
- Shells are not only interactive tools; they also interpret **shell scripts**:
  - Text files containing shell commands
  - Support programming constructs:
    - Variables
    - Loops and conditionals
    - Functions
    - I/O operations
  - While syntax varies between shells, they perform similar roles.

## Users and Groups

- Every user on a UNIX/Linux system is identified in two ways:
- **Login name (username)** – human-readable identifier
- **User ID (UID)** – numeric identifier used internally by the kernel
- The **kernel ultimately uses the UID**, not the username, for all permission checks.
- Each user is defined by a line in `/etc/passwd`, which contains:
  - **Username**
  - **UID** – unique numeric identifier
  - **Primary Group ID (GID)** – the user’s default group
  - **Home directory** – initial working directory after login
  - **Login shell** – program started to interpret commands
- Historically, encrypted passwords were stored directly in `/etc/passwd`
  - For security reasons, modern systems store them in:
    - **/etc/shadow** (readable only by privileged users)
  - `/etc/passwd` remains world-readable so basic user information is accessible.
- **Groups** are a mechanism for **organizing users** and **controlling access** to files and system resources.
- Purpose:
  - Share files among a team or project
  - Assign permissions collectively rather than per-user
  - Simplify system administration
- Group Membership:
  - Early UNIX: one group per user
  - BSD introduced **multiple group membership**
  - POSIX standardized this behavior
- A user therefore has:
  - **One primary group**
  - **Zero or more supplementary groups**
- Each group is defined by a line in `/etc/group`, containing:
  - **Group name**
  - **Group ID (GID)**
  - **User list** – users who belong to the group _in addition to_ those whose primary GID matches
- The **superuser** is a special account with unrestricted system privileges.
- **Key Characteristics**
  - **UID = 0**
  - Usually named **root**
  - Bypasses almost all permission checks
- The superuser can:
  - Read, write, and delete any file
  - Send signals to any process
  - Bind to privileged network ports
  - Load kernel modules
  - Change ownership and permissions
  - Manage users, groups, and system configuration
- Modern systems often use **sudo** to grant temporary root privileges instead of direct root login
- The kernel does **not** care about usernames or group names
  - It operates solely on: **UIDs** and **GIDs**
  - User and group names are resolved to numbers in user space (e.g., via libc)

## Single Directory Hierarchy, Directories, Links, and Files

- UNIX systems use **one global directory hierarchy** rooted at: `/`
  - All files and directories exist somewhere under `/`
  - There are **no per-disk trees** like `C:\`, `D:\` in Windows
  - Different disks and devices are _mounted_ into this single tree ❕
- 👉 This design lets programs access files uniformly, regardless of where the data is physically stored.
- In UNIX, **everything is a file**, but each file has a **type**:

| Type             | Description                                  |
| ---------------- | -------------------------------------------- |
| Regular file     | Ordinary data (text, binaries, images, etc.) |
| Directory        | Maps names → files                           |
| Character device | Byte-stream devices (e.g., terminals)        |
| Block device     | Block-oriented devices (e.g., disks)         |
| FIFO (pipe)      | Interprocess communication                   |
| Socket           | Network or local IPC                         |
| Symbolic link    | Path reference to another file               |

- The word _file_ refers to **any of these types**, not just regular files.
- A **directory** is a special file whose contents are a table: `filename → inode`
  - This association is called a **link** (specifically, a _hard link_).
- A file may have **multiple hard links**
- Each hard link gives the file another name
- Removing a filename removes a link, not necessarily the file
- The file is deleted only when: link count = 0 **and** no process has it open.
- Every directory contains:
  - `.` → link to itself
  - `..` → link to parent directory
- For the root directory: `/.. == /`
- A **symbolic link** is a special file whose contents are a **pathname**: `symlink ──► "target/path"`
  | Hard Link | Symbolic Link |
  | -------------------------------------- | ------------------------- |
  | Points directly to inode | Contains target pathname |
  | Cannot cross filesystems | Can cross filesystems |
  | Cannot reference directories (usually) | Can reference directories |
  | Target must exist | Can be dangling |
- The kernel automatically follows symlinks during pathname resolution
- This may occur **recursively**
- The kernel limits recursion depth to prevent loops
- If the target does not exist → **dangling symlink**
- Filenames has a **max length**: **255 bytes** (on most Linux filesystems)
- Cannot contain: `/` (path separator) OR `\0` (string terminator)
- Portable filename character set: `[-._a-zA-Z0-9]`, using this avoids:
  - Shell interpretation problems
  - Regex conflicts
  - Quoting/escaping issues
- Avoid:
  - Filenames beginning with `-`
  - Characters with special shell meaning (`*`, `?`, space, `$`, etc.)
- A **pathname** locates a file within the hierarchy: `/usr/include/sys/types.h`
  - Directory part: `/usr/include/sys`
  - Base (filename): `types.h`
- **Absolute paths** starts with `/`
  - Independent of current directory
  - Examples: `/home/mtk/.bashrc` OR `/usr/include`
- **Relative Path**
  - Does **not** start with `/`
  - Interpreted relative to the process’s current working directory
  - Examples: `include/sys/types.h` OR `../mtk/.bashrc`
- Each process has a (CWD) **current working directory**:
  - Inherited from parent process
  - **Login shell** starts in **user’s home** directory
  - Relative pathnames are always resolved using the CWD.
- Each file has: **Owner UID** and a **Group GID**.
  - These are used by the kernel for access control.
  - UNIX divides users into three classes:

| Class | Applies to              |
| ----- | ----------------------- |
| User  | File owner              |
| Group | Members of file’s group |
| Other | Everyone else           |

- Each class has **three permission bits**:

| Bit         | File Meaning       | Directory Meaning            |
| ----------- | ------------------ | ---------------------------- |
| Read (r)    | Read file contents | List filenames               |
| Write (w)   | Modify contents    | Create/delete/rename entries |
| Execute (x) | Execute file       | Access files inside (search) |

- For directories:
  - `r` without `x` → names visible, files inaccessible
  - `x` without `r` → files accessible if names are known
  - `w` without `x` → **useless**

## File I/O Model

- A defining idea in UNIX is **I/O universality**: > The same system calls are used to perform I/O on _all_ file types.
  - The core system calls (`open()`, `read()`, ..) work on: regular files, directories, pipes, sockets, terminals ....
  - The **kernel** translates these calls into:
    - Filesystem operations (for disk files)
    - Device driver operations (for devices)
  - Programs do not need to know _what kind_ of file they are using
  - Tools become composable (e.g., pipes, redirection)
  - “Everything is a file” is not just a slogan — it’s an API guarantee.
- The kernel presents most files as: a **sequential stream of bytes**
  - For seekable objects (regular files, block devices):
    - Random access: `lseek(fd, offset, whence);`
    - This does **not** apply to: pipes, fifos, sockets and terminals ➡️ These are **stream-only** objects.
- UNIX has **no concept of “lines”**
  - Newlines (`'\n'`, ASCII 10) are just bytes
  - Line interpretation happens in **user space**
- There is **no EOF character**
  - EOF is detected when: `read(fd, buf, size) == 0`
  - This design works for both: Files AND Streams (e.g., pipes, sockets)
- A **file descriptor (FD)** is:
  - A small non-negative integer
  - An index into a per-process file descriptor table
  - Obtaining a file descriptor: `int fd = open("file.txt", O_RDONLY);`
  - The FD refers to an **open file description** in the kernel, which includes:
    - File offset
    - Access mode
    - Status flags
- Every process started by a shell normally has:
  | FD | Name | Purpose |
  | --- | ------ | ------------ |
  | 0 | stdin | Input |
  | 1 | stdout | Output |
  | 2 | stderr | Error output |
- Typically:
  - Connected to a terminal
  - Or redirected to files/pipes
- **stdio vs System Calls**
- **System Calls**: Low-level, unbuffered ➡️ `open(), read(), write(), close()`
- **Stdio Library**: Higher-level, buffered ➡️ `fopen(), fread(), fprintf(), fgets(), fclose()`
  ```sh
  stdio
    ↓
  read / write
    ↓
  kernel
    ↓
  filesystem / device driver
  ```
- The stdio library:
  - Adds buffering
  - Adds formatting
  - Adds portability
- `stderr` is **unbuffered** by default, it ensures error messages appear immediately

## Programs

- **Filters** are programs that read data from standard input (`stdin`), transform it, and write the result to standard output (`stdout`).
  - Common examples include `cat`, `grep`, `tr`, `sort`, `wc`, `sed`, and `awk`.
- **Command-line arguments** allow C programs to receive input parameters when they are run.
  - They are accessed via `int main(int argc, char *argv[])`, where `argc` is the number of arguments and `argv` is an array of strings containing those arguments. `argv[0]` holds the name used to invoke the program.

## Processes

- A **process** is a running instance of a program. The kernel loads the program into memory, allocates resources, and tracks metadata such as PID, user IDs, and state. Processes are how the kernel shares and manages system resources.
- **Process memory layout** consists of four segments:
  - **Text**: program instructions (read-only, shared).
  - **Data**: static/global variables.
  - **Heap**: dynamically allocated memory.
  - **Stack**: function calls, local variables, and control data.
- **Process creation and execution**:
  - `fork()` creates a child process as a copy of the parent.
  - The child may continue executing the same code or replace its memory image using `execve()` (and related `exec*()` functions) to run a new program.
- **Process identifiers**:
  - Each process has a unique **PID** and a **PPID** identifying its parent.
- **Process termination**:
  - A process ends via `_exit()/exit()` or by receiving a signal.
  - It yields a **termination status**, which the parent can retrieve with `wait()`.
  - Convention: status `0` = success; nonzero = error.
- **Process credentials**:
  - **Real UID/GID**: who owns the process.
  - **Effective UID/GID**: permissions used for access checks.
  - **Supplementary GIDs**: additional group memberships.
  - Credentials are inherited from the parent process.
- **Privileged processes**:
  - Traditionally, processes with effective UID `0` (root) bypass permission checks.
  - Privilege can also be gained via **set-user-ID** programs.
- **Capabilities (Linux-specific)**:
  - Root privileges are split into fine-grained **capabilities** (e.g., `CAP_KILL`).
  - A process may hold only the capabilities it needs, improving security.
- **The `init` process**:
  - PID 1, created at boot from `/sbin/init`.
  - Ancestor of all processes, runs as superuser, cannot be killed, and manages system services.
- **Daemon processes**:
  - Long-lived background processes without a controlling terminal (e.g., `syslogd`, `httpd`).
- **Environment variables**:
  - Each process has an environment (name–value pairs) inherited via `fork()` and usually preserved across `exec()`.
  - Used to pass configuration (e.g., `HOME`, `PATH`).
- **Resource limits**:
  - Controlled via `setrlimit()`, with **soft** and **hard** limits on resources like CPU time, memory, and open files.
  - Limits are inherited by child processes; shells can set them with `ulimit`.

## Memory Mappings

- `mmap()` creates a new **memory mapping** in a process’s virtual address space.
- There are two main types of mappings:
  - **File mappings**: map a region of a file into memory, allowing the file’s contents to be accessed and modified via memory operations. Pages are loaded on demand.
  - **Anonymous mappings**: not backed by a file; pages are initialized to zero and used for memory allocation.
- **Sharing behavior**:
  - Mappings can be shared between processes if they map the same file region or via inheritance after `fork()`.
  - **Private mappings**: changes are visible only within the process and are not written back to the file.
  - **Shared mappings**: changes are visible to all processes sharing the mapping and are propagated to the underlying file.
- **Uses of memory mappings** include:
  - Loading executable code (text segments).
  - Allocating zero-initialized memory.
  - Performing file I/O via memory-mapped I/O.
  - Enabling interprocess communication through shared memory.

## Static and Shared Libraries

- An **object library** is a collection of compiled object code providing reusable functions for programs. UNIX systems support **static** and **shared** libraries.
- **Static Libraries**
  - Contain bundled object modules.
  - During linking, required modules are copied into the executable.
  - Resulting program is **statically linked**.
  - **Disadvantages**:
    - Duplicates code across executables, wasting disk space.
    - Multiple running programs each keep their own copy in memory.
    - Updating a library requires recompiling the library **and relinking all dependent programs**.
- **Shared Libraries**
  - Executables reference the library instead of copying its code.
  - At run time, the **dynamic linker** loads the library and resolves symbols.
  - A single copy of the library code is shared by all running programs.
  - 👍:
    - Saves disk and memory space.
    - Updating the library automatically updates all programs that use it (on next execution).

## Interprocess Communication and Synchronization

- A Linux system runs many processes, some of which must **communicate and synchronize** to work together.
- Using **disk files** for communication is often too slow and inflexible, so UNIX/Linux provides multiple **IPC mechanisms**:
  - **Signals**: Notify a process that an event has occurred.
  - **Pipes and FIFOs**: Transfer data between processes (pipes often used via `|` in shells).
  - **Sockets**: Exchange data between processes on the same machine or across a network.
  - **File locking**: Prevent concurrent processes from conflicting when accessing files.
  - **Message queues**: Send and receive discrete messages between processes.
  - **Semaphores**: Synchronize process actions.
  - **Shared memory**: Allow processes to share memory directly, enabling fast communication.
- The variety of IPC mechanisms reflects UNIX’s historical evolution across different systems and standards, leading to **overlapping functionality** (e.g., FIFOs from System V and sockets from BSD both enable local interprocess data exchange).

## Signals

- **Signals** are often described as _software interrupts_ used to notify a process that an event or exceptional condition has occurred.
- Each signal represents a specific event and is identified by an integer constant named `SIGxxxx`.
- Signals may be sent by:
  - The **kernel** (e.g., on invalid memory access or timer expiration),
  - **Another process** with appropriate permissions,
  - Or the **process itself**.
- Examples of events that generate signals:
  - User presses **Ctrl-C**,
  - A child process terminates,
  - A process **timer expires**,
  - A process accesses invalid memory.
- Signals can be sent from the shell using `kill`, or programmatically using the `kill()` system call.
- When a signal is delivered, a process may:
  - Ignore the signal,
  - Be terminated by the signal,
  - Be suspended until resumed by another signal.
- For most signals, a process can override the default action by:
  - Ignoring the signal, or
  - Installing a **signal handler**, a programmer-defined function that runs automatically when the signal is delivered.
- A signal is **pending** if it has been generated but not yet delivered.
- Signals are normally delivered when the process runs.
- A process can **block signals** using a signal mask; blocked signals remain pending and are delivered once unblocked.

## Threads

- A **process can contain multiple threads of execution**.
- Threads within a process:
  - Share the **same virtual address space**, program code, global data, and heap.
  - Each have their **own stack** for local variables and function call state.
- Threads communicate easily via **shared global variables**.
- To safely coordinate access to shared data, threading APIs provide:
  - **Mutexes** (mutual exclusion locks),
  - **Condition variables**.
- Threads may also use standard **IPC and synchronization mechanisms** (e.g., signals, shared memory).
- 👍 of threads\*\*
  - **Efficient data sharing** compared to separate processes.
  - **Natural fit for certain algorithms** that are easier to express concurrently.
  - **Better performance and scalability**, as multithreaded programs can automatically exploit **parallelism on multi-CPU systems**.

## Process Groups and Shell Job Control

- **Each command executed by the shell runs in its own process**.
- A pipeline (e.g. `ls -l | sort -k5n | less`) is implemented as **multiple processes**, one per command, connected via pipes.
- Most modern shells (except the original Bourne shell) support **job control**, allowing users to manage multiple commands or pipelines interactively.
- In job-control shells:
  - All processes in a pipeline are placed into a **process group** (also called a _job_).
  - A single command still forms its own process group.
  - All processes in the group share the same **process group ID (PGID)**, which equals the PID of the **process group leader**.
- The kernel can perform operations—especially **signal delivery**—on an entire process group at once.
- Job-control shells rely on this to **suspend, resume, or terminate** all processes in a pipeline together (e.g., via Ctrl-C or Ctrl-Z).

## Sessions, Controlling Terminals, and Controlling Processes

- A **session** is a collection of **process groups (jobs)**.
- All processes in a session share the same **session ID (SID)**.
- The **session leader** is the process that creates the session; its **PID becomes the SID**.
- **Job-control shells** rely heavily on sessions:
  - The shell is the **session leader**.
  - All jobs (process groups) started by the shell belong to the same session.
- A session usually has one **controlling terminal**, established when the session leader first opens a terminal device.
  - A terminal can be the controlling terminal of **only one session**.
  - The session leader becomes the **controlling process** for that terminal.
  - If the terminal disconnects (e.g., window closed), the controlling process receives **SIGHUP**.
- At any time, **one process group** in the session is the **foreground process group**:
  - It can read input from the terminal.
  - It receives terminal-generated signals.
- If the user presses:
  - **Ctrl-C** → signal sent to kill the foreground process group.
  - **Ctrl-Z** → signal sent to suspend (stop) the foreground process group.
- A session may contain **multiple background process groups**, typically created using `&`.
- **Job-control shells** provide commands to:
  - List jobs
  - Send signals to jobs
  - Move jobs between foreground and background

## Pseudoterminals

- A **pseudoterminal (PTY)** consists of a connected pair of virtual devices: **Master** && **Slave**
- Toegther, they form a **bidirectional IPC channel**.
- **How PTYs work**
  - The **slave device behaves like a real terminal**.
  - A **terminal-oriented program** (e.g., a shell) attaches to the slave.
  - Another program (the **driver**) attaches to the master and:
    - Sends input to the slave (as if typed by a user),
    - Receives output from the slave (as if reading from a terminal).
  - All normal **terminal input and output processing** (newline mapping, echoing, line discipline, etc.) is applied.
- The program connected to the **master acts like the user**, controlling the terminal-oriented program indirectly.
- **Common uses**
  - Terminal emulator windows in **X Window systems**.
  - **Remote login tools** such as `telnet` and `ssh`.
  - Any application that needs to **simulate a terminal** programmatically.

## Date and Time

- **Two kinds of time matter to a process:**
  - **1. Real time**
    - Measures actual elapsed (wall-clock) time.
    - Can be:
      - **Calendar time**: seconds since **00:00 UTC, January 1, 1970** (the **UNIX Epoch**).
      - **Elapsed time**: time since a process started.
  - **2. Process (CPU) time**
    - Total CPU time used by a process.
    - Split into:
      - **User CPU time**: time spent executing the program’s own code.
      - **System CPU time**: time spent executing kernel code on behalf of the process (e.g., system calls).
- **time command**: displays **real time**, **user CPU time**, and **system CPU time** for executing a command or pipeline.

## Client–Server Architecture

- A **client–server application** is split into two processes:
  - **Client**: sends requests for a service.
  - **Server**: processes requests and returns responses.
- Communication may be a single request/response or an ongoing dialogue.
- Clients usually interact with users; servers manage **shared resources**.
- Many clients typically communicate with one or a few server instances.
- Client and server can run on the **same machine** or on **different hosts over a network**, using IPC mechanisms.
- **Typical server services include:**
  - Database or shared information access
  - Remote file access
  - Business logic processing
  - Access to shared hardware (e.g., printers)
  - Web content serving
- **Benefits of centralizing services in a server:**
  - **Efficiency**: shared resources reduce duplication.
  - **Control, coordination, and security**: centralized management prevents conflicts and restricts access.
  - **Heterogeneous support**: clients and servers can run on different hardware and operating systems.

## Realtime

- **Realtime applications** must respond to events within a guaranteed time limit (a deadline), often interacting with sensors and hardware.
- Examples include **industrial automation**, **ATMs**, and **aircraft navigation systems**.
- The key requirement is **predictable, bounded response time**, not just fast execution.
- Providing realtime guarantees requires **operating system support**, which can conflict with traditional multiuser time-sharing goals.
- Classic UNIX systems are **not realtime OSes**, though realtime variants exist.
- **Linux** has realtime variants, and modern kernels are progressively adding **native realtime support**.
- **POSIX.1b realtime extensions include:**
  - Asynchronous I/O
  - Shared memory and memory-mapped files
  - Memory locking
  - Realtime clocks and timers
  - Alternative scheduling policies
  - Realtime signals
  - Message queues and semaphores
  - While not all UNIX systems are strictly realtime, **most support many of these POSIX.1b features**, including Linux.

## The /proc File System

- Linux provides a **virtual file system** mounted at `/proc` that exposes **kernel data structures** as files and directories.
- `/proc` allows **viewing and sometimes modifying system attributes** in a convenient file-like interface.
- Each running process has a directory `/proc/PID` containing information about that process.
- Most `/proc` files are **human-readable** and can be **parsed by scripts** or programs using standard file I/O.
- **Modifying `/proc` files** usually requires **privileged access**.
- `/proc` is **Linux-specific**; it is not standardized and provides **implementation-specific details** useful for system programming and monitoring.
