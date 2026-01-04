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

## Virtual Memory Management

#### Core Concept: Locality of Reference
Programs typically exhibit:
- **Spatial locality**: Access memory addresses close to recently accessed ones (e.g., sequential code or data traversal).
- **Temporal locality**: Re-access the same memory soon after (e.g., loops).

➡️ A program can run even if **only part** of its address space is in RAM at once.

#### Virtual Memory Organization
- Memory is divided into fixed-size **pages** (virtual pages).
- RAM is divided into matching **page frames**.
- Only a subset of a process’s pages (the **resident set**) is in RAM at any time.
- Unused pages are stored in the **swap area** on disk.
- When a process accesses a non-resident page → **page fault** → kernel loads the page from swap into RAM (suspending the process briefly).

#### Page Table
- Each process has its own **page table** mapping virtual addresses to:
  - Physical page frame (if resident), or
  - Location on disk (swap).
- Unused virtual address ranges have **no page-table entry** → access causes **SIGSEGV** (segmentation fault).

#### Dynamic Virtual Address Space Changes
The valid virtual address range grows/shrinks during a process’s lifetime via:
- Stack growth (downward)
- Heap allocation (`brk()`/`sbrk()`, `malloc()` family – Chapter 7)
- System V shared memory (`shmat()`/`shmdt()` – Chapter 48)
- Memory mappings (`mmap()`/`munmap()` – Chapter 49)

#### Hardware Support
- **Paged Memory Management Unit (PMMU)**:
  - Translates virtual → physical addresses.
  - Notifies kernel on page faults.

#### Advantages of Virtual Memory

| Advantage                              | Explanation                                                                 |
|----------------------------------------|-----------------------------------------------------------------------------|
| **Process isolation**                  | Each process’s page table points to distinct physical pages → one process cannot read/modify another’s memory or the kernel’s. |
| **Memory sharing**                     | Page tables of different processes can point to the **same** physical pages:<br>• Implicit: multiple processes running same program share **text segment** (code) and shared libraries.<br>• Explicit: via `shmget()`/`mmap()` for IPC. |
| **Memory protection**                  | Page-table entries mark pages as read/write/execute.<br>Different processes can have different permissions on shared pages (e.g., one read-only, another read-write). |
| **Simplified programming**             | Programmers, compiler, linker don’t worry about physical RAM layout.       |
| **Faster program startup / smaller footprint** | Only needed pages loaded → program can be larger than RAM.                 |
| **Better CPU utilization**             | More processes fit in RAM simultaneously → higher chance CPU always has work. |

## The Stack and Stack Frames

- On **Linux x86-32** (and most UNIX/Linux architectures):
  - Stack resides at the **high end** of the process's virtual address space.
  - Grows **downward** (toward lower addresses, i.e., toward the heap).
- A dedicated CPU register — the **stack pointer (SP)** — always points to the **current top** of the stack.
- Despite growing downward, the "top" is still the **growing end** (abstract view).
- Exception: `HP PA-RISC` Linux uses an **upward-growing** stack 🤷‍♀️.
- Growth direction is a **hardware/implementation detail**.

#### Stack Behavior in Virtual Memory
- Logically: Stack **grows** when functions are called (new frames added) and **shrinks** when they return (frames removed).
- Physically: On most systems, the **stack segment** only grows in virtual memory — it does **not shrink** when frames are deallocated.
  - Freed space is simply **reused** for future stack frames.
  - The kernel may extend the stack segment downward as needed (via page faults).

#### User Stack vs. Kernel Stack
- **User stack**: Described here — in user virtual memory, used for normal function calls.
- **Kernel stack**: Separate per-process stack in **protected kernel memory**.
  - Used when executing system calls (kernel code can't safely use the user stack).
  - Fixed size (typically 8–16 KB per thread on Linux).

#### Contents of a Stack Frame
Each time a function is called, a new **stack frame** (also called activation record) is pushed onto the stack. It typically contains:

1. **Function parameters and local (automatic) variables**
   - Created automatically on function entry.
   - Destroyed automatically on return → distinguishes them from `static`/`global` variables (which persist).

2. **Call linkage / return information**
   - Saved CPU registers, especially:
     - **Program counter (PC / EIP)** — address to return to after function completes.
     - **Frame pointer (FP / EBP)** — points to base of current frame (used for accessing locals/params).
     - Other caller-saved registers.
   - Enables proper return to the calling function and restoration of its state.

#### Nested and Recursive Calls
- Functions can call other functions → multiple stack frames exist simultaneously.
- Recursive functions → multiple frames for the **same function**.

#### Key Conceptual Diagram (Typical x86-32 Process Virtual Memory Layout)
```
High addresses
+-------------------+  ← Stack top (grows downward)
|   Stack frames    |
|     ...           |
|  square() frame   |
|  main() frame     |
+-------------------+
|     (unused)      |
+-------------------+  ← Heap (grows upward via brk/sbrk or mmap)
|       Heap        |
+-------------------+
|   Data segment    |  (.data, .bss – globals, static vars)
+-------------------+
|   Text segment    |  (executable code)
+-------------------+
Low addresses (0x0)
```

## Command-Line Arguments (argc, argv)

Every C program must have a `main()` function, which is the **entry point** for execution.

There are two standard forms:
```c
int main(int argc, char *argv[]);
// or equivalently:
int main(int argc, char **argv);
```

- **`argc`** (argument count): Integer indicating the **number of command-line arguments**, including the program name.
- **`argv`** (argument vector): Array of pointers to **null-terminated strings**.
  - `argv[0]`: Conventionally the **program name** (as invoked).
  - `argv[1]` to `argv[argc-1]`: The actual arguments.
  - `argv[argc]`: Always `NULL` (terminates the array).

Example:
```bash
$ ./myprog hello world "foo bar"
```
→ `argc = 4`  
→ `argv[0] = "./myprog"`  
→ `argv[1] = "hello"`  
→ `argv[2] = "world"`  
→ `argv[3] = "foo bar"`  
→ `argv[4] = NULL`

#### Useful Trick: Multiple Names for One Program
- Create **hard links** (or symbolic links) to the same executable.
- The program checks `argv[0]` and behaves differently depending on the name used to invoke it.

**Real-world example**:
- `gzip`, `gunzip`, and `zcat` are often **links to the same binary**.
- The program examines `argv[0]` to decide whether to compress, decompress, or print to stdout.

⚠️ Must handle unexpected link names (e.g., user creates their own link).

#### Memory Layout of `argc` and `argv`
- The strings pointed to by `argv` (including `argv[0]`) are stored in a **contiguous memory region** just above the **process stack**.
- The `argv` array itself (list of pointers) and the **environment list** (`environ`) also reside in this area.
- All strings are **null-terminated** (`\0`).

#### Limitations and Portability
- `argc` and `argv` are only directly available in `main()`.
- To use them elsewhere:
  - **Portable**: Pass `argv` as a parameter or store in a global variable.
- **Non-portable methods** (Linux/glibc-specific):
  - Read `/proc/PID/cmdline` or `/proc/self/cmdline` (null-separated arguments).
  - Use glibc globals (with `#define _GNU_SOURCE`):
    - `program_invocation_name` → full path used to invoke program.
    - `program_invocation_short_name` → basename (e.g., "myprog").

#### Size Limits on Arguments + Environment
- Defined by **ARG_MAX** (in `<limits.h>`) or via `sysconf(_SC_ARG_MAX)`.
- SUSv3 minimum: **4096 bytes** (`_POSIX_ARG_MAX`).
- Most systems allow much more.
- Whether overhead (pointers, alignment, null bytes) counts toward limit is implementation-defined.

**Linux-specific details**:
- Historically: Fixed at **131,072 bytes** (32 pages on x86-32), including overhead.
- Since **kernel 2.6.23**: Limited to **¼ of the soft RLIMIT_STACK** at time of `execve()`.
  - Allows much larger argument/environment space on modern systems.

##  Environment List

- Each process has an **environment list**: an array of strings in the form `name=value`.
- These are called **environment variables**.
- When a process is created (via `fork()`), it **inherits a copy** of its parent's environment.
- Changes to the environment are **private**:
  - Parent → child: one-way transfer at creation time.
  - After creation, each process can modify its own copy independently.

#### Common Uses
1. **Shell → child programs**:
   - Shell places values (e.g., `PATH`, `HOME`, `SHELL`) in its environment.
   - All commands executed by the shell inherit these (e.g., many programs use `SHELL` to know which shell to spawn).

2. **Configuring library/application behavior**:
   - Example: `POSIXLY_CORRECT` changes `getopt()` parsing rules.
   - Allows user control without recompiling or relinking.

#### Shell Commands for Managing Environment
| Shell Type               | Add permanently to shell environment                     | One-shot (for single command)       | Remove             | Display            |
|--------------------------|----------------------------------------------------------|-------------------------------------|--------------------|--------------------|
| Bash/Korn/Bourne         | `export NAME=value` or `export NAME` after setting       | `NAME=value program`                | `unset NAME`       | `printenv` or `env` |
| C shell                  | `setenv NAME value`                                      | Not directly supported              | `unsetenv NAME`    | `printenv`         |

- `env` command: Run a program with a **modified** environment (add/remove variables).
  - `env`: Print all environment variables (same as printenv)
  - `env VAR=value command`: Run command with modified environment
  - `env -i command`: Run command with empty environment
  - `env -u VAR command`: Run command with VAR removed from environm
- `printenv`: Show current environment (unsorted order — implementation-dependent).

Linux-specific: View any process's environment via `/proc/PID/environ` (null-separated strings).

#### Accessing the Environment in C Programs

1. **Global variable `environ`** (preferred, portable):
   ```c
   extern char **environ;  // NULL-terminated array of "name=value" strings
   ```
   - Like `argv`, but no count variable (loop until `NULL`).
   - Example: Listing 6-3 prints all environment variables (equivalent to `printenv`).

2. **Third argument to `main()`** (widely available but **non-standard** — avoid):
   ```c
   int main(int argc, char *argv[], char *envp[])
   ```
   - `envp` works like `environ`, but scope limited to `main()`.

3. **Retrieve single variable**:
   ```c
   char *getenv(const char *name);  // Returns pointer to value or NULL
   ```
   - **Do not modify** the returned string (it's part of the actual environment).
   - On some systems, return value may be in static buffer → copy if preserving across calls.

#### Modifying the Environment

| Function                      | Purpose                                                                 | Key Notes                                                                 |
|-------------------------------|-------------------------------------------------------------------------|---------------------------------------------------------------------------|
| `int putenv(char *string)`    | Add/modify: pass `"name=value"` string                                  | String becomes part of environment → **do not** use stack variables       |
| `int setenv(const char *name, const char *value, int overwrite)` | Add/modify safely                                                       | Allocates memory, copies strings → safe to modify inputs afterward        |
| `int unsetenv(const char *name)` | Remove variable                                                         | Name must not contain '='                                                 |
| `int clearenv(void)`          | Erase entire environment (non-standard, glibc/BSD)                      | Sets `environ = NULL` → subsequent `setenv()`/`putenv()` rebuild it       |

- `setenv()` is generally **preferred** over `putenv()` because it's safer.
- `clearenv()` is useful for security (e.g., in set-user-ID programs) but **not in SUSv3**.
  - SUSv3 alternative: iterate over `environ` and `unsetenv()` each variable.

#### Memory Layout Note
- `argv` strings, `environ` strings, and the pointer arrays reside in a **contiguous region** just above the stack.
- Subject to size limits (related to `ARG_MAX` and stack limit).

## Performing a Nonlocal Goto: setjmp() and longjmp()

- Standard C `goto` is limited: **cannot jump out of the current function**.
- Common scenario: **error handling in deeply nested calls**.
  - Detect error deep in call stack.
  - Want to abort the entire operation and resume in a high-level function (e.g., `main()`).
- Normal approach: return error codes up the call chain → verbose, repetitive.
- `setjmp()`/`longjmp()` allow **direct jump** back to a known point, simplifying some error-handling logic.
```c
#include <setjmp.h>

int setjmp(jmp_buf env);          // Returns 0 directly, nonzero after longjmp()
void longjmp(jmp_buf env, int val);  // Never returns
```

#### How They Work
1. **`setjmp(env)`**:
   - Saves current execution context (program counter, stack pointer, registers) into `env`.
   - Returns **0** when called directly.
   - Marks a **jump target**.

2. **`longjmp(env, val)`**:
   - Restores context from `env`.
   - **Unwinds the stack** (removes intervening frames).
   - Makes it appear as if `setjmp()` is **returning again**, this time with value `val`.
   - If `val == 0`, `setjmp()` returns **1** instead (to avoid confusion with initial call).

➡️ Execution continues **immediately after the original `setjmp()` call**, as if it just returned.

#### Typical Usage Pattern
```c
jmp_buf env; // Usually declared global or passed down (to be visible to deep functions).

if (setjmp(env) == 0) {
    // Normal execution path
    deep_function();  // may call longjmp(env, 1) on error
} else {
    // Execution resumes here after longjmp()
    printf("Recovered from deep error\n");
}
```

#### Restrictions on `setjmp()` Usage (SUSv3 / C99)
`setjmp()` can only appear in very simple contexts:
- Alone in `if`, `switch`, `while`, etc.
- As `!setjmp(...)`
- In comparison with integer constant: `setjmp(env) != 0`
- As standalone call (not part of larger expression)

**Invalid**:
```c
int x = setjmp(env);  // WRONG – not allowed by standard
```

👉 Reason: Normal function implementation of `setjmp()` can't save temporary values in complex expressions — optimizer might break restoration.

#### Dangers and Pitfalls

1. **Abusing `longjmp()` into a dead function**:
   - Call `setjmp()` in function X → return from X → call `longjmp()` to X's `env`.
   - Stack frame for X is gone → **undefined behavior** (crash, infinite loop, corruption).

2. **Nested signal handlers**:
   - `longjmp()` from a signal handler invoked during another handler → **undefined behavior**.

3. **Interaction with optimizing compilers** (Listing 6-6):
   - Optimizers assume normal control flow.
   - After `longjmp()`, local variables may be restored from **old register values**.
   - Example:
     - Normal compile: variables retain post-`setjmp()` values.
     - Optimized compile: non-`volatile` variables revert to pre-`setjmp()` values.
   - **Fix**: Declare affected local variables as **`volatile`**:
     ```c
     volatile int vvar;     // Prevents register optimization
     register int rvar;     // Explicit register hint – still risky
     ```

   - gcc warns with `-Wextra`: "variable might be clobbered by `longjmp()`".

#### Best Practices and Warnings
- Use `volatile` on all local variables (of optimizable types: int, pointers, etc.) in the function containing `setjmp()`.
- **Avoid `setjmp()`/`longjmp()` when possible**:
  - Makes code harder to read and maintain (nonlocal control flow).
  - Reduces portability due to optimizer issues.
  - Prefer structured error handling (return codes, exceptions in other languages).
- Still useful in rare cases:
  - Signal handlers (`sigsetjmp()`/`siglongjmp()` – covered later).
  - Certain low-level or performance-critical code.
