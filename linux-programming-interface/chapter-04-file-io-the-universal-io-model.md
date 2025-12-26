# File I/O: The Universal I/O Model

- **File Descriptors (FDs)**: All I/O system calls use **file descriptors** — small, nonnegative integers — to identify open files.  
  Every process has its own independent set of file descriptors.  
- They can refer to **any type** of open file: regular files, pipes, sockets, terminals, devices, etc.
- By convention, every process inherits three standard file descriptors from its parent (usually the shell):

| FD | Purpose           | POSIX name       | stdio stream | Typical default (in shell) |
|----|-------------------|------------------|--------------|----------------------------|
| 0  | Standard input    | STDIN_FILENO     | stdin        | Keyboard/terminal          |
| 1  | Standard output   | STDOUT_FILENO    | stdout       | Screen/terminal            |
| 2  | Standard error    | STDERR_FILENO    | stderr       | Screen/terminal            |

- Use the **POSIX names** (`STDIN_FILENO`, etc.) from `<unistd.h>` instead of hardcoding 0, 1, 2 — better for readability and portability.
- The **stdio streams** (`stdin`, `stdout`, `stderr`) initially point to FDs 0, 1, 2, but you can reassign them with `freopen()` — after which the underlying FD may change (so don't assume `stdout` is always FD 1).
<p align="center"><img src="./assets/syscalls.png" width="400px" height="auto"></p>

- These are the fundamental system calls for file I/O (higher-level libraries like stdio wrap them):
  - **`open(pathname, flags, mode)`**  
    Opens (or creates) a file and returns a new file descriptor.  
    - `flags`: Bitmask controlling mode (read, write, append, create, etc.).  
    - `mode`: Permissions if the file is created (ignored otherwise).
  - **`read(fd, buffer, count)`**  
    Reads **up to** `count` bytes from the open file into `buffer`.  
    Returns the actual number of bytes read (0 = EOF).
  - **`write(fd, buffer, count)`**  
    Writes **up to** `count` bytes from `buffer` to the file.  
    Returns the actual number of bytes written (may be less than `count`).
  - **`close(fd)`**  
    Closes the file descriptor and frees kernel resources.

## Universality of I/O

- The same **four system calls** — `open()`, `read()`, `write()`, and `close()` — work for **all types of files**:
  - Regular files
  - Devices (e.g., terminals, disks, serial ports)
  - Pipes, FIFOs, sockets, etc.
- This means a program written using only these calls becomes **device-independent** — it can read from or write to almost anything without needing special code for each type.
- The simple `copy` program from Listing 4-1 (your basic `cp` clone) works in many contexts:

| Command                              | What it does                                      |
|--------------------------------------|---------------------------------------------------|
| `./copy test test.old`               | Copies one regular file to another                |
| `./copy a.txt /dev/tty`              | Copies file contents to the current terminal      |
| `./copy /dev/tty b.txt`              | Copies input from the terminal to a regular file  |
| `./copy /dev/pts/16 /dev/tty`        | Copies input from another terminal (e.g., pts/16) to the current terminal |

- Every **file system** and **device driver** in the kernel implements the same set of I/O system calls.
- Device-specific details are hidden inside the kernel.
- Applications stay simple and portable — they rarely need to care about the underlying type.
- For device-specific or advanced features not covered by the basic I/O model, use the `ioctl()` system call .
