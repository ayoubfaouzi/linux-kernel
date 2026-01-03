# Processes

## Processes and Programs

- **Program**:
  - A **static file** on disk containing all information needed to construct a running instance at runtime.
  - One program can be used to create **many independent processes** (e.g., multiple instances of `/bin/ls` running simultaneously).
- **Process**:
  - An **abstract entity** managed by the kernel.
  - A **running instance** of a program — dynamic, with allocated system resources.
  - More formally: "A process is an abstract entity, defined by the kernel, to which system resources are allocated in order to execute a program."

#### Contents of a Program (Executable File)
A typical modern UNIX executable (usually in **ELF** format on Linux) contains:

| Component                              | Purpose                                                                 |
|----------------------------------------|-------------------------------------------------------------------------|
| **Binary format identification**       | Metadata indicating the file format (e.g., ELF). Allows kernel to parse the file. Historically: a.out, COFF; now mostly ELF. |
| **Machine-language instructions**      | The actual compiled code (the algorithm) that the CPU executes.        |
| **Program entry-point address**        | Location of the first instruction to execute when the process starts.  |
| **Data**                               | Initial values for variables + literal constants (e.g., string literals). |
| **Symbol and relocation tables**       | Names and locations of functions/variables — used for debugging and dynamic linking. |
| **Shared-library / dynamic-linking info** | List of required shared libraries and path to the dynamic linker (e.g., `/lib64/ld-linux-x86-64.so.2`). |
| **Other metadata**                     | Various info used by the kernel/loader to build the process image.     |

#### Kernel's View of a Process
From the kernel’s perspective, a process consists of:

1. **User-space memory**:
   - Program code (text segment)
   - Initialized and uninitialized data
   - Heap, stack, shared libraries

2. **Kernel data structures** maintaining process state, including:
   - Various **IDs** (PID, PPID, UID, GID, etc.)
   - **Virtual memory tables** (page tables)
   - **File descriptor table** (open files)
   - **Signal handling** information (pending signals, signal handlers)
   - **Resource usage and limits** (CPU time, open files, etc. — see `getrusage()`, `setrlimit()`)
   - **Current working directory**
   - Scheduling information, process credentials, and many other attributes

## Process ID and Parent Process ID

- **Definition**: A **positive integer** that **uniquely identifies** a process on the system.
- **Uses**:
  - Send signals: `kill(pid, signal)` (Section 20.5).
  - Create process-unique identifiers (e.g., filenames like `/tmp/myapp_12345.pid`).
- **No fixed relationship** between a program and its PID (except system processes like `init`, PID 1).
- PIDs are **recycled**: Assigned sequentially from low numbers.

#### `getpid()` – Get Calling Process's PID
```c
#include <unistd.h>
pid_t getpid(void);
```
- **Always succeeds** — returns the PID of the calling process.
- `pid_t`: SUSv3 integer type for PIDs (usually `int`).

#### PID Limits and Assignment (Linux-Specific)
| Linux Version | Max PID | Notes |
|---------------|---------|-------|
| **2.4 and earlier** | 32,767 (`PID_MAX`) | Fixed limit |
| **2.6+ (32-bit)** | 32,767 (default) | Adjustable via `/proc/sys/kernel/pid_max` (max 32,768) |
| **2.6+ (64-bit)** | Up to 2²² ≈ **4 million** | Adjustable via `/proc/sys/kernel/pid_max` |

**Assignment behavior**:
- New processes get the **next available** PID.
- When max reached → counter resets to **300** (not 1, to skip system PIDs like `init`).
- Avoids wasting time searching low-numbered PIDs used by daemons.

#### Parent Process ID (PPID)
- Every process (except `init`) has a **parent** — the process that created it.
- Forms a **tree structure** rooted at `init` (PID 1).
  - View with `pstree(1)` command.
- **Orphaned children** (parent dies) are **adopted by `init`** → `getppid()` returns 1 (details in Section 26.2).
```c
#include <unistd.h>
pid_t getppid(void);
```
- **Always succeeds** — returns the PID of the calling process's **parent**.
- Check parent via `/proc/PID/status` → `PPid:` field.

## Memory Layout of a Proces

This section describes the **virtual memory layout** of a typical UNIX/Linux process, divided into distinct **segments** (regions with different purposes and properties).

#### Process Memory Segments

| Segment                  | Contents                                                                 | Key Properties                                                                 |
|--------------------------|--------------------------------------------------------------------------|---------------------------------------------------------------------------------|
| **Text**                 | Machine-language instructions (the compiled program code)                | **Read-only** (prevents accidental modification)<br>**Sharable** (multiple processes running the same program share one physical copy) |
| **Initialized data**     | Global and static variables that are **explicitly initialized** in code  | Values loaded directly from the executable file at program start                |
| **Uninitialized data** (bss) | Global and static variables **not explicitly initialized** (default to 0) | Initialized to **zero** by the kernel before program start<br>No space needed in executable file (only size recorded) → saves disk space |
| **Stack**                | Stack frames for function calls: local (automatic) variables, arguments, return addresses | **Dynamically grows/shrinks** downward<br>One frame per active function (details in Section 6.5) |
| **Heap**                 | Dynamically allocated memory (via `malloc()`, `new`, etc.)               | Grows upward from the "program break" (top of heap)<br>Managed at runtime       |

#### Why Separate Initialized and Uninitialized Data?
- **Executable file efficiency**: Uninitialized variables are all zero → no need to store zeros on disk.
- Executable only records **location and size** of bss segment → program loader allocates and zeros it at runtime.

#### Visual Layout (Typical on Linux/x86-64)
```
  Higher addresses
+-------------------+  
|  Kernel (mapped   | but not accessible to program. `proc/kallsyms`
|  into process     | provides addresses of kernel symbols in this
|  virtual memory)  | region ( /proc/ksyms in kernel 2.4 and earlier)
+-------------------+  
|  argv, environ    | 
+-------------------+  ← Top of virtual address space
|     Stack         |  (grows downward)
+-------------------+
|     ...           |
+-------------------+
|      Heap         |  (grows upward) ← program break (brk/sbrk)
+-------------------+  <- end
| Uninitialized data (bss)
+-------------------+  <- edata
| Initialized data  |
+-------------------+  <- etext
|     Text          |  (read-only, sharable code)
+-------------------+
Lower addresses (0x0)
```
