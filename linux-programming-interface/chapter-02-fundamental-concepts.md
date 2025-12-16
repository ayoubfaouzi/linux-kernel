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
  - “Everything is a file” is not just a slogan—it’s an API guarantee
- The kernel presents most files as: a **sequential stream of bytes**
  - For seekable objects (regular files, block devices):
    - Random access: `lseek(fd, offset, whence);`
    - This does **not** apply to: pipes, fifos, sockets and terminals :arrow ➡️ These are **stream-only** objects.
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

| FD  | Name   | Purpose      |
| --- | ------ | ------------ |
| 0   | stdin  | Input        |
| 1   | stdout | Output       |
| 2   | stderr | Error output |

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
- **Why UNIX I/O Scales So Well**
  - Everything uses the same I/O primitives
  - Redirection and pipes are trivial
  - Tools compose naturally
```sh
  cat file | grep foo | sort | uniq
```
- Each program:
  - Reads from FD 0
  - Writes to FD 1
  - Knows nothing about pipes
