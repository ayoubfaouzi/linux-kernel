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
- Library functions fall into three broad categories:
  1. **System-call–like behavior**
    - Return **–1 on error** and set `errno`.
    - Example: `remove()`
    - Handle exactly like system calls.
  2. **Different return value, but still use `errno`**
    - Example: `fopen()` returns `NULL` on error.
    - `errno` indicates the cause.
    - `perror()` / `strerror()` are appropriate.
  3. **Do not use `errno`**
    - Error reporting is function-specific.
    - The manual page documents how errors are reported.
    - **Do not use `errno`, `perror()`, or `strerror()`** for these functions.
