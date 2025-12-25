# System Programming Concepts

## System Calls

- A **system call** is a controlled interface that allows a user-space program to request services from the kernel (e.g., process creation, I/O, IPC).
- System calls switch execution from **user mode to kernel mode** so the kernel can safely access protected resources.
- The set of system calls is **fixed**, each identified internally by a **unique number**, though programs refer to them by name.
- Arguments and results are passed between **user space and kernel space**.
- **How a system call works (conceptually):**
  - A program calls a **C library wrapper function**.
  - The wrapper places arguments and the system call number into specific CPU registers.
  - A special instruction (e.g., `int 0x80` or the faster `sysenter`) triggers a **trap** into the kernel.
  - The kernel’s system call handler:
    - Saves CPU state,
    - Validates the system call number and arguments,
    - Invokes the corresponding **system call service routine** (e.g., `sys_execve`) by indexing `sys_call_table`,
    - Returns a result.
  - Control returns to user mode, and the wrapper function reports success or failure.
- **Error handling:**
  - On success, system calls return a **nonnegative value**.
  - On error, they return a **negative value** corresponding to an `errno`.
  - The C library wrapper converts this into `-1` and sets the global `errno` variable.
- **Performance considerations:**
  - System calls incur **measurable overhead** due to mode switching and validation.
  - Even simple system calls are significantly slower than ordinary C function calls.
  <p align="center"><img src="./assets/syscalls.png" width="400px" height="auto"></p>

## Library Functions

- A **library function** is a function provided by the **standard C library** (e.g., string handling, file I/O helpers, memory allocation).
- Many library functions **do not use system calls at all** (e.g., `strlen()`, `strcmp()`).
- Some library functions are **built on top of system calls**:
  - `fopen()` → uses `open()`
  - `printf()` → adds formatting and buffering on top of `write()`
  - `malloc()` / `free()` → manage memory bookkeeping over low-level mechanisms like `brk()` (and `mmap()` in modern implementations)
- Library functions often provide a **more convenient, safer, and higher-level interface** than raw system calls.

## The Standard C Library; The GNU C Library ( glibc)

- Different UNIX systems have different C library implementations.
- On Linux, the most widely used implementation is **glibc**.
- Alternative C libraries (often for embedded systems): **uClibc** OR **diet libc**
- Most Linux applications (and this book’s examples) assume **glibc**.
- **Determining the glibc version**:
  - **Shell**: You can execute the `glibc` shared object directly: When we run the library as an executable, it displays various text,
    including its version number.
  - Use `ldd` on a dynamically linked executable: `ldd myprog | grep libc`
  - During **compile time**: glibc defines: `__GLIBC__` and `__GLIBC_MINOR__`
    - ⚠️ These only reflect the **build system**, not necessarily the system where the binary runs.
  - During **Run-time** (recommended), Use:

    ```c
    #include <gnu/libc-version.h>

    const char *gnu_get_libc_version(void);
    ```

  - Returns a pointer to a **statically allocated string**, e.g. `"2.12"`
  - Example: `printf("glibc version: %s\n", gnu_get_libc_version());`
  - Alternative: `confstr()`
  - You can also query: `_CS_GNU_LIBC_VERSION`, this returns a string such as: `glibc 2.12`

## Handling Errors from System Calls and Library Functions

- Nearly all **system calls and library functions return a status** indicating success or failure.
- **Always check these return values**; skipping checks often leads to hard-to-debug errors.
- A few system calls **never fail** (e.g., `getpid()`, `_exit()`), so checking them is unnecessary.
- Most system calls indicate failure by returning **–1**.

```c
fd = open(pathname, flags, mode);
if (fd == -1) {
    /* handle error */
}
```

- On failure, the kernel sets the global variable **`errno`** to a positive error code.
  - Declared in `<errno.h>`
  - Error names begin with **E** (e.g., `EINTR`, `ENOENT`)
- Check `errno` **only after** detecting an error via the return value.

```c
cnt = read(fd, buf, numbytes);
if (cnt == -1) {
    if (errno == EINTR)
        fprintf(stderr, "read interrupted by signal\n");
}
```

- ⚠️ Notes about `errno`:
  - Successful calls **do not reset errno**.
  - Some successful calls may still set `errno` (per POSIX).
  - Therefore: **check the return value first, then errno**.
- Some calls (e.g., `getpriority()`) may legitimately return –1 on success 🤷‍♀️.

```c
errno = 0;
ret = getpriority(...);
if (ret == -1 && errno != 0) {
    /* error occurred */
}
```

- **`perror()`** prints a custom message followed by a description of `errno`: `perror("open");`
- **`strerror()`** returns a string describing a given error number: `fprintf(stderr, "%s\n", strerror(errno));`
- The string from `strerror()` may be statically allocated and overwritten.
- If the error number is unknown, it returns `"Unknown error nnn"` (or `NULL` on some systems).
- Both are **locale-sensitive**, so messages appear in the user’s language.

Library functions fall into three broad categories:

- **System-call–like behavior**
  - Return **–1 on error** and set `errno`.
  - Example: `remove()`
  - Handle exactly like system calls.
- **Different return value, but still use `errno`**
  - Example: `fopen()` returns `NULL` on error.
  - `errno` indicates the cause.
  - `perror()` / `strerror()` are appropriate.
- **Do not use `errno`**
  - Error reporting is function-specific.
  - The manual page documents how errors are reported.
  - **Do not use `errno`, `perror()`, or `strerror()`** for these functions.

## Notes on the Example Programs in This Book

- **Command-line options**:
  - UNIX programs commonly use **short options** (`-x`) and optional arguments.
  - GNU tools also support **long options** (`--option`).
  - Options are typically parsed using **`getopt()`**.
  - Programs usually support `--help` to display usage information.
- Example programs include a **shared header** that:
  - Includes frequently used system headers
  - Defines a Boolean type
  - Provides utility macros (e.g., min/max)
  - This reduces repetition and shortens example code.
- To simplify consistent error handling, a set of helper functions is used:
  - **For system calls / errno-based errors**
    - `errMsg()` – prints an error message (includes errno info), continues execution.
    - `errExit()` – prints error message and terminates (optionally dumps core).
    - `err_exit()` – like `errExit()` but uses `_exit()` and does not flush stdio.
    - `errExitEN(errnum, ...)` – like `errExit()`, but uses an explicit error number (important for **POSIX threads**, which return error codes instead of setting `errno`).
      **For non-errno or usage errors**
  - `fatal()` – reports general errors not tied to `errno`, then exits.
  - `usageErr()` – reports incorrect command usage and exits.
  - `cmdLineErr()` – reports command-line argument errors and exits.
- These helpers:
  - Print both the **symbolic errno name** (e.g., `EACCES`) and its description
  - Ensure consistent formatting and termination behavior
  - Avoid common mistakes with `errno`, especially in multithreaded programs
- `ename.c.inc` defines an array that maps numeric `errno` values to their symbolic names.
  - Helps bridge the gap between:
    - `strerror()` (human-readable text)
    - Manual pages (symbolic names like `EAGAIN`)
- Architecture-specific because errno values vary.
- Some errors share values (e.g., `EAGAIN` and `EWOULDBLOCK`).
- For **parsing numeric command-line arguments**, two helper functions are used instead of `atoi()` / `strtol()`:
  - `getInt(arg, flags, name)`
  - `getLong(arg, flags, name)`
  - 👍:
    - Validate input (reject non-numeric strings)
    - Automatically print clear error messages and exit
    - Optional range checks via flags:
      - Base selection (decimal, octal, hex)
      - Non-negative or strictly positive constraints

## Portability Issues

- UNIX APIs evolved from **multiple standards** (POSIX, X/Open, BSD, System V, GNU).
- Header files may expose **different functions, constants, and structures** depending on which standard you want to target.
- **Feature test macros tell the C library which API set you want.**
- They must be defined **before including any headers**.

### How to define them❓

- **In source code**
  ```c
  #define _XOPEN_SOURCE 600
  #include <unistd.h>
  ```
- **Or at compile time (preferred)**
  ```bash
  cc -std=c99 -D_XOPEN_SOURCE=600 prog.c
  ```
- The following FTM are defined by POSIX / SUS and are portable across UNIX systems.

#### `_POSIX_C_SOURCE`

- Controls which **POSIX version** you target.
- Common values:

| Value  | Meaning                   |
| ------ | ------------------------- |
| 1      | POSIX.1-1990              |
| 199309 | POSIX realtime (POSIX.1b) |
| 199506 | POSIX threads (POSIX.1c)  |
| 200112 | POSIX.1-2001              |
| 200809 | POSIX.1-2008              |

👉 Use when you want **POSIX-only** APIs (no XSI extensions).

#### `_XOPEN_SOURCE`

- Controls **Single UNIX Specification (SUS)** conformance (POSIX + XSI extensions).
- Common values:

| Value | Meaning         |
| ----- | --------------- |
| 500   | SUSv2           |
| 600   | SUSv3 (UNIX 03) |
| 700   | SUSv4 (UNIX 08) |

👉 **Defining `_XOPEN_SOURCE` automatically enables the corresponding POSIX level**.

**Best portable choice today:**

```bash
-D_XOPEN_SOURCE=700
```

#### glibc-specific macros (non-portable)

- These work on Linux/glibc but should be avoided for strict portability.
  | Macro          | Purpose                                   |
  | -------------- | ----------------------------------------- |
  | `_BSD_SOURCE`  | BSD extensions                            |
  | `_SVID_SOURCE` | System V extensions                       |
  | `_GNU_SOURCE`  | Everything (POSIX + BSD + GNU extensions) |
- ⚠️ `_GNU_SOURCE` is convenient but **locks you to glibc/Linux**.
- When compiling **without `-std=c99` or `-ansi`**, GCC defines:
  - POSIX
  - BSD
  - SVID
  - A recent `_POSIX_C_SOURCE`
- That’s why many Linux programs “just work” — but **portability suffers**.
- All example code is guaranteed to compile with: `cc -std=c99 -D_XOPEN_SOURCE=600`
- And each function prototype explicitly states **which macro is required**.

### Standard System Data Types

- We must NOT use `int`, `long`, etc., because:
  - Their size varies across architectures (32-bit vs 64-bit)
  - The same concept may use different underlying types on different systems
  - Types may change across kernel/libc versions
- **Example (Linux):**
  - `UID` was 16-bit on Linux 2.2
  - `UID` is 32-bit on Linux 2.4+
- POSIX/SUS defines standard typedefs, usually ending in `_t`, here are few examples:

  | Type      | Meaning     |
  | --------- | ----------- |
  | `pid_t`   | Process ID  |
  | `uid_t`   | User ID     |
  | `gid_t`   | Group ID    |
  | `off_t`   | File offset |
  | `size_t`  | Object size |
  | `ssize_t` | Signed size |

```c
#include <sys/types.h>

pid_t pid = getpid();
printf("%d\n", pid);            // Wrong (non-portable), pid may not be int
printf("%ld\n", (long) pid);    // Portable convention

// Special case: `off_t`, may be `long long`
printf("%lld\n", (long long) offset);
```

- C99 defines:
  - `%zd` for `size_t`
  - `%jd` for `intmax_t`
- These are **not universally supported** on all UNIX systems, so the book avoids them.

### Miscellaneous Portability Issues

- POSIX **does not guarantee field order** in structures.
```c
struct sembuf {
    unsigned short sem_num;
    short sem_op;
    short sem_flg;
};

// Non-portable
struct sembuf s = { 3, -1, SEM_UNDO }; 

// Portable:
struct sembuf s;
s.sem_num = 3;
s.sem_op  = -1;
s.sem_flg = SEM_UNDO;

// OR, use C99 designated initializer
struct sembuf s = {
    .sem_num = 3,
    .sem_op  = -1,
    .sem_flg = SEM_UNDO
};
```

- Some macros are **widely available but not standardized** (e.g., `WCOREDUMP`).
```c
// Portable usage
#ifdef WCOREDUMP
    if (WCOREDUMP(status)) { ... }
#endif
```

- Some systems require extra headers that Linux does not.
- The book marks these as:

```c
#include <sys/types.h>   /* For portability */

// Best practice, even if Linux doesn’t require it.
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
```
