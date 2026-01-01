# File I/O: Further Details

- **Atomicity of System Calls**: Every system call is executed **atomically** by the kernel.
  - The entire sequence of steps inside the system call completes as a **single, uninterruptible operation**.
  - No other process or thread can interfere midway through the call.
- This guarantee is crucial for reliable concurrent programming.
- **Race Condition**: A bug where the outcome of a program depends unpredictably on the **relative timing** or **scheduling order** of multiple processes/threads accessing shared resources.
- The result varies depending on which process gets CPU time first — hence "race".

#### Example 1: Creating a File Exclusively (Without Atomicity)

Goal: Ensure only one process creates a file (e.g., for lock files, temp files, or single-instance checks).

**Bad approach** (Listing 5-1 — vulnerable to race):

```c
fd = open(path, O_WRONLY);           // Step 1: Check if file exists
if (fd != -1) {                       // Exists → error/close
    ...
} else if (errno == ENOENT) {
    // WINDOW OF VULNERABILITY
    fd = open(path, O_WRONLY | O_CREAT, mode);  // Step 2: Create it
    printf("I created the file exclusively!\n"); // MAY BE FALSE!
}
```

**The race**:

- Process A: `open()` fails → file doesn't exist.
- Scheduler interrupts A → switches to Process B.
- Process B: does the same → creates the file.
- Scheduler resumes A → A now creates/truncates the file again.
- **Both processes think they exclusively created it** → incorrect and dangerous.

Demonstrated with artificial `sleep(5)` delay: two concurrent runs both claim exclusive creation.

**Correct solution**: 👉 `O_CREAT | O_EXCL`

```c
fd = open(path, O_WRONLY | O_CREAT | O_EXCL, mode);
```

- The kernel **atomically** checks for existence **and** creates the file.
- If file already exists → `open()` fails with `EEXIST`.
- No race window → safe in concurrent environments.

#### Example 2: Appending to a File

Goal: Multiple processes safely append to a shared log file without overwriting each other's data.

**Bad approach** (vulnerable to race):

```c
lseek(fd, 0, SEEK_END);   // Move to end of file
write(fd, buf, len);      // Append data
```

**The race**:

- Process A: `lseek()` to end (say offset 1000).
- Scheduler switches to Process B.
- Process B: `lseek()` to end (still 1000) → writes its data.
- Scheduler resumes A → A writes at offset 1000 → **overwrites B's data**.

**Correct solution**: 👉 Open the file with `O_APPEND`

```c
fd = open(path, O_WRONLY | O_APPEND, ...);
```

- The kernel **atomically** seeks to the current end and writes the data.
- Every `write()` automatically appends, even if multiple processes write concurrently.
- No need for explicit `lseek()`.

**Caveat**: On some filesystems (e.g., older NFS), `O_APPEND` may not be truly atomic — kernel falls back to non-atomic `lseek()+write()`, reintroducing the race risk.

## File Control Operations: fcntl()

`fcntl()` is a **multiplexed** system call that provides a wide range of operations not covered by the basic universal I/O model.

```c
#include <fcntl.h>
int fcntl(int fd, int cmd, ... /* arg */);
```

- **Returns**:
  - Value depends on the specific `cmd` (often 0 on success, a new flag value, or another integer)
  - **–1** on error (sets `errno`)
- **Args**:
  - `fd` — An open file descriptor (must already be opened via `open()`, `pipe()`, `socket()`, etc.).
  - `cmd` — An integer specifying the operation to perform (many possible values).
  - Third argument (`arg`) — Optional and type-varying:
    - Can be an **integer**, a **pointer to a structure**, or omitted entirely.
    - The kernel interprets its type and meaning based on the value of `cmd`.
- **Common uses include**:
  - Retrieving or modifying **file descriptor flags** (e.g., close-on-exec)
  - Retrieving or modifying **file status flags** (e.g., `O_APPEND`, `O_NONBLOCK`)
  - Duplicating file descriptors (`dup()` and `dup2()` are implemented via `fcntl()` in some cases)
  - File locking (advisory and mandatory)
  - Managing signals for asynchronous I/O
  - And many others (covered in later chapters)

## Open File Status Flags

`fcntl()` **retrieve** and **modify** the open file status flags (and access mode) of an already-open file descriptor,

```c
int flags = fcntl(fd, F_GETFL);  // No third argument needed
if (flags == -1) errExit("fcntl");
```

- Returns the current **open file status flags** (the same bits set via `flags` in `open()`).
- Includes both the **access mode** (`O_RDONLY`, `O_WRONLY`, `O_RDWR`) and other status flags (`O_APPEND`, `O_NONBLOCK`, etc.).

**Testing individual flags** (bitwise AND):

```c
if (flags & O_SYNC)          // Check if synchronous writes enabled
    printf("writes are synchronized\n");

if (flags & O_APPEND)
    printf("append mode enabled\n");
```

**Extracting the access mode** (not single bits):

```c
int accessMode = flags & O_ACCMODE;  // Mask to isolate access mode bits

if (accessMode == O_WRONLY || accessMode == O_RDWR)
    printf("file is writable\n");

if (accessMode == O_RDONLY)
    printf("file is read-only\n");
```

- `O_ACCMODE` masks out everything except the access mode (values 0, 1, or 2).

> If the program was compiled for large file support (see Section 5.10), `O_LARGEFILE` is **always** set in the returned flags, even though SUSv3 says only explicitly set flags should be visible.

#### Modifying Flags: `F_SETFL`

```c
if (fcntl(fd, F_SETFL, new_flags) == -1)
    errExit("fcntl");
```

- Third argument: the new flag value (integer).

**Changeable flags** (Linux):

- `O_APPEND`
- `O_NONBLOCK` (nonblocking I/O)
- `O_NOATIME` (don't update access time)
- `O_ASYNC` (signal on I/O possible)
- `O_DIRECT` (bypass buffer cache)

**Non-changeable flags** (ignored if set):

- Access mode (`O_RDONLY`, `O_WRONLY`, `O_RDWR`)
- `O_CREAT`, `O_EXCL`, `O_TRUNC`, etc.
- `O_SYNC`/`O_DSYNC` (on Linux; some other UNIX systems allow changing these)

**Typical pattern to modify a flag** (e.g., enable `O_APPEND`):

```c
int flags = fcntl(fd, F_GETFL);
if (flags == -1) errExit("fcntl - F_GETFL");

flags |= O_APPEND;                 // Turn on O_APPEND (or use &= ~flag to turn off)

if (fcntl(fd, F_SETFL, flags) == -1)
    errExit("fcntl - F_SETFL");
```

#### Why Use `F_SETFL`?

Useful when the program **did not control the original `open()`**:

- Standard file descriptors (0, 1, 2) inherited from the shell.
- File descriptors from other system calls:
  - `pipe()` → returns two descriptors for a pipe
  - `socket()` → returns a socket descriptor
  - `accept()` → returns a connected socket descriptor
- In these cases, you may want to change behavior (e.g., make a socket nonblocking or enable append mode on a pipe).

## Relationship Between File Descriptors and Open Files

🧠 There is not a **1:1 relationship** between file descriptors and open files. Multiple file descriptors (in the same or different processes) can refer to the **same open file**, with important sharing implications.

The kernel maintains **three separate tables** to manage open files:

1. **Per-process file descriptor table**
   - One per process.
   - Each entry corresponds to one file descriptor (0, 1, 2, ...).
   - Stores:
     - File descriptor flags (currently only **close-on-exec**, covered in Section 27.4)
     - Pointer to the **open file description**

2. **System-wide open file description table** (sometimes called "open file table")
   - Global across all processes.
   - Each entry represents one **open file description** (also called "open file handle").
   - Stores information shared by all descriptors referring to this open file:
     - Current **file offset** (updated by `read()`, `write()`, `lseek()`)
     - **Open file status flags** (e.g., `O_APPEND`, `O_NONBLOCK`, `O_ASYNC`, from `open()` or `fcntl()`)
     - File **access mode** (`O_RDONLY`, `O_WRONLY`, `O_RDWR`)
     - Signal-driven I/O settings
     - Pointer to the file's **i-node**

3. **File system i-node table** (per file system)
   - One entry per file on disk.
   - Stores persistent and in-memory file attributes:
     - File type, permissions, owner, size, timestamps
     - Pointers to file data blocks
     - List of locks on the file
     - In-memory additions: count of open descriptions referencing it, device IDs, ephemeral data

#### Common Scenarios:

<p align="center"><img src="./assets/file-descriptors-vs-open-files.png" width="400px" height="auto"></p>

| Scenario | How it happens | What is shared |
|----------|----------------|----------------|
| **Two descriptors in same process point to same open file description** (e.g., fd 1 and fd 20 in process A → description 23) | `dup()`, `dup2()`, or `fcntl()` with `F_DUPFD` | File offset, status flags |
| **Descriptors in different processes point to same open file description** (e.g., fd 2 in process A and fd 2 in process B → description 73) | After `fork()` (child inherits parent's descriptors), or passing descriptor via UNIX domain socket | File offset, status flags |
| **Descriptors refer to different open file descriptions but same i-node** (e.g., fd 0 in process A and fd 3 in process B → i-node 1976) | Each process independently called `open()` on the same file (or one process opened it twice) | Only file content and metadata (size, permissions, etc.) — **not** offset or status flags |

#### Key Implications of Sharing

| Shared via same open file description | Not shared (private) |
|---------------------------------------|----------------------|
| **File offset** — changes via one descriptor (e.g., `lseek()`, `read()`, `write()`) are visible through all others | **File descriptor flags** (close-on-exec) — per descriptor, per process |
| **Open file status flags** — changes via `fcntl()` `F_SETFL` affect all descriptors sharing the description |  |
| Applies **across processes** if they share the description (e.g., parent/child after `fork()`) |  |

#### Why This Matters

- Enables efficient duplication of descriptors (`dup()` family).
- Explains why `fork()` children share file offsets with parents.
- Clarifies what is shared vs. independent when multiple descriptors point to the same file.

This model is fundamental to understanding later topics like:

- Duplicating descriptors (`dup()`, `dup2()`, `fcntl()` `F_DUPFD`)
- Behavior after `fork()`
- File locking
- Passing file descriptors between processes

## Duplicating File Descriptors

#### Why Duplication Matters
- Shell redirection `> results.log 2>&1` makes fd 2 point to the same open file as fd 1.
- Simply opening the file twice is **wrong** because:
  - The two descriptors would have **separate file offsets** → risk of overwriting output.
  - The destination may not be a regular file (e.g., a pipe: `./myscript 2>&1 | less`).
- 👍 Duplication ensures both descriptors share the **same open file description** (same offset, same status flags).

#### 1. `dup()`
```c
#include <unistd.h>
int dup(int oldfd);
```
- Returns the **lowest unused** file descriptor that refers to the same open file description as `oldfd`.
- Example (assuming only 0,1,2 open):
  ```c
  int newfd = dup(1);  // newfd will be 3, duplicate of stdout
  ```

#### 2. `dup2()`
```c
#include <unistd.h>
int dup2(int oldfd, int newfd);
```
- Makes `newfd` a duplicate of `oldfd`.
- If `newfd` is already open, it is **silently closed first** (better to close explicitly for safety).
- Returns `newfd` on success.
- Special cases:
  - If `oldfd` invalid → fails with `EBADF`, `newfd` not closed.
  - If `oldfd == newfd` → does nothing, returns `newfd`.

**Shell `2>&1` equivalent**:
```c
dup2(1, 2);  // Make fd 2 duplicate fd 1
```

**Manual equivalent of `dup()` using `close()` + `dup2()`** (to force reuse of fd 2):
```c
close(2);
dup(1);  // Now reuses fd 2 (assuming 0 was open)
```

#### 3. `fcntl()` with `F_DUPFD`
```c
int newfd = fcntl(oldfd, F_DUPFD, startfd);
```
- Duplicates `oldfd` using the **lowest unused fd ≥ startfd**.
- Useful when you need the new descriptor in a specific range.
- `dup()` and `dup2()` can always be implemented with `close()` + `fcntl()`, but `dup`/`dup2` are more concise.

#### 4. Modern Variants with Close-on-Exec Control

| Call                  | Description                                                                 | Linux Version | Standard |
|-----------------------|-----------------------------------------------------------------------------|---------------|----------|
| `dup3(oldfd, newfd, flags)` | Like `dup2()`, but with `flags` (currently only `O_CLOEXEC`)                | 2.6.27+       | Linux-specific |
| `fcntl(oldfd, F_DUPFD_CLOEXEC, startfd)` | Like `F_DUPFD`, but sets close-on-exec on new fd                           | 2.6.24+       | SUSv4    |

- `O_CLOEXEC` / `FD_CLOEXEC`: Ensures the new descriptor is **closed** on `exec()` (prevents accidental leakage to child programs; same rationale as `O_CLOEXEC` in `open()`).

#### Properties of Duplicated Descriptors
- **Shared** (via same open file description):
  - File offset
  - Open file status flags (`O_APPEND`, `O_NONBLOCK`, etc.)
- **Private** (per descriptor):
  - File descriptor flags (especially **close-on-exec** — new duplicates have it **off** by default, unless using `_CLOEXEC` variants)

#### Summary Table of Duplication Methods

| Function              | New FD Choice                          | Closes existing FD? | Sets close-on-exec? | Typical Use |
|-----------------------|----------------------------------------|---------------------|---------------------|-------------|
| `dup(oldfd)`          | Lowest unused                          | No                  | No                  | Simple duplicate |
| `dup2(oldfd, newfd)`  | Exactly `newfd`                        | Yes (silently)      | No                  | Shell redirections (e.g., 2>&1) |
| `fcntl(..., F_DUPFD, start)` | Lowest ≥ `start`                 | No                  | No                  | Range control |
| `fcntl(..., F_DUPFD_CLOEXEC, start)` | Lowest ≥ `start`         | No                  | Yes                 | Thread-safe / secure |
| `dup3(oldfd, newfd, flags)` | Exactly `newfd`                  | Yes                 | Yes (if `O_CLOEXEC`) | Modern secure `dup2` |


## File I/O at a Specified Offset: pread() and pwrite()

These two system calls provide **random-access I/O without modifying the file offset** — essentially a combination of `lseek()` + `read()`/`write()` in a single atomic operation.

```c
#include <unistd.h>

ssize_t pread(int fd, void *buf, size_t count, off_t offset);
ssize_t pwrite(int fd, const void *buf, size_t count, off_t offset);
```
- **Returns** (same as `read()`/`write()`):
  - Number of bytes transferred (≥ 0) on success
  - 0 for `pread()` on EOF
  - –1 on error

#### Key Differences from `read()`/`write()`

| Feature                  | `read()` / `write()`                  | `pread()` / `pwrite()`                          |
|--------------------------|---------------------------------------|-------------------------------------------------|
| I/O location             | Current file offset                   | Explicit `offset` parameter                     |
| File offset after call   | Advanced by bytes transferred         | **Unchanged**                                   |
| Number of system calls   | 1 (plus `lseek()` if seeking)         | 1 (seek + I/O combined)                         |
| Atomicity w.r.t. offset  | Separate `lseek()` can race           | Atomic (no race window)                         |

#### Equivalent (but non-atomic) Sequence for `pread()`

```c
off_t orig = lseek(fd, 0, SEEK_CUR);   // Save current offset
lseek(fd, offset, SEEK_SET);           // Move to desired position
ssize_t s = read(fd, buf, count);      // Perform I/O
lseek(fd, orig, SEEK_SET);             // Restore original offset
```
`pread()/pwrite()` does all this **atomically** in one kernel call.

#### Main Advantages

1. **Thread Safety in Multithreaded Programs**
   - All threads in a process share the **same file descriptor table** → same file offset for each open file description (see Section 5.4).
2. **Process Safety with Shared Open File Descriptions**
   - If multiple processes share the same open file description (e.g., after `fork()`), the file offset is shared.
   - Again, `pread()`/`pwrite()` prevent races.
3. **Slight Performance Benefit**
   - One system call instead of two (`lseek()` + `read()`/`write()`).
   - Minor savings in system call overhead (usually negligible compared to actual disk I/O time).


## Scatter-Gather I/O: readv() and writev()

These system calls perform **scatter-gather I/O**: transferring data to/from **multiple non-contiguous buffers** in a **single atomic system call**.

```c
#include <sys/uio.h>

ssize_t readv(int fd, const struct iovec *iov, int iovcnt);
ssize_t writev(int fd, const struct iovec *iov, int iovcnt);

// Linux 2.6.30+ and modern BSDs
ssize_t preadv(int fd, const struct iovec *iov, int iovcnt, off_t offset);
ssize_t pwritev(int fd, const struct iovec *iov, int iovcnt, off_t offset);

// The `struct iovec`
struct iovec {
    void  *iov_base;   // Starting address of buffer
    size_t iov_len;    // Length of buffer
};
```
- `iov` is an array of these structures.
- `iovcnt` is the number of elements in the array (≥ 1).

**Limits**:
- SUSv3 requires at least 16 (`_POSIX_IOV_MAX`).
- Linux kernel limit: 1024 (`UIO_MAXIOV` → defined as `IOV_MAX` in `<limits.h>`).
- glibc wrapper handles larger vectors by falling back to a single `read()`/`write()` with a temporary buffer.

#### `readv()` – Scatter Input
- Reads a **contiguous sequence** of bytes from the file.
- **Scatters** them sequentially into the buffers: fills `iov[0]` completely, then `iov[1]`, etc.
- **Atomic**: Entire transfer (file → all buffers) is one uninterruptible operation.
  - Guarantees contiguous bytes from file, even if other threads/processes change the shared file offset concurrently.
- Returns: total bytes read (may be less than requested → check return value).
- Partial read possible → last buffer may be partially filled.

#### `writev()` – Gather Output
- **Gathers** data from all buffers in order (`iov[0]`, `iov[1]`, ...).
- Writes them as a **contiguous sequence** to the file.
- **Atomic**: Entire transfer (all buffers → file) is one uninterruptible operation.
  - Ensures all data appears contiguously on disk, without interleaving from other processes/threads.
- Returns: total bytes written (may be less than requested → check return value).

#### `preadv()` and `pwritev()` (Linux ≥ 2.6.30, modern BSDs)
- Same as `readv()`/`writev()`, but perform I/O at explicit `offset`.
- **Do not change** the current file offset.
- Combine benefits of:
  - Scatter-gather (multiple buffers)
  - Positioned I/O (`pread`/`pwrite` style → thread-safe, no offset races)

#### Advantages Over Alternatives

| Approach                              | Atomic? | System Calls | Memory Copy Needed? | Convenience |
|---------------------------------------|---------|--------------|---------------------|-------------|
| Single `writev()`                     | Yes     | 1            | No                  | High        |
| Multiple `write()` calls              | No      | Many         | No                  | Low         |
| Allocate big buffer + single `write()`| Yes     | 1            | Yes (user-space copy) | Low         |

`readv()`/`writev()` are **faster** and **cleaner** because:
- Fewer system calls (see Section 3.1 on system call overhead).
- No manual buffer allocation or copying in user space.
- Atomicity prevents races in concurrent environments.

#### Use Cases
- Network servers: gathering headers + payload + trailers for output.
- Reading/writing structured records that span multiple non-adjacent buffers.
- Multithreaded applications: `preadv()`/`pwritev()` for thread-safe random scatter-gather I/O.
- High-performance I/O where minimizing syscalls and copies matters.

## Truncating a File: truncate() and ftruncate()

These two system calls **change the size of a regular file** to exactly the specified `length` (in bytes).

```c
#include <unistd.h>

int truncate(const char *pathname, off_t length);
int ftruncate(int fd, off_t length);
```
- Both return **0** on success, **–1** on error (sets `errno`).

#### Behavior Based on `length`
| Current file size vs. `length` | Effect |
|--------------------------------|--------|
| File **longer** than `length`  | Excess data beyond `length` is **discarded** (lost forever). |
| File **shorter** than `length` | File is **extended**. The new space is filled with:<br>• **Null bytes** (on most filesystems), **or**<br>• A **file hole**.|

#### Differences Between the Two Calls

| Call          | How file is specified                  | Requirements                              | Notes |
|---------------|----------------------------------------|-------------------------------------------|-------|
| `truncate()`  | By **pathname** (string)               | File must be writable; symbolic links are **dereferenced** | Unique: only system call that modifies a file's contents **without** needing an open descriptor (via `open()`, `pipe()`, etc.) |
| `ftruncate()` | By open **file descriptor** (`fd`)     | `fd` must refer to a file opened for writing | Does **not** change the current file offset |

#### Example Usage
```c
// Empty a log file without opening it
truncate("app.log", 0);

// Or using an open descriptor
int fd = open("data.bin", O_WRONLY | O_CREAT, 0644);
ftruncate(fd, 1024 * 1024);  // Ensure at least 1 MiB (extends with hole if needed)
```

##  Nonblocking I/O

The `O_NONBLOCK` flag changes the behavior of file I/O from **blocking** (default) to **nonblocking**.

#### Two Main Effects of `O_NONBLOCK`

1. **During `open()`**
   - If the file cannot be opened **immediately**, `open()` fails and returns **–1** (with `errno` set, typically `EAGAIN` or `ENOENT` depending on the case) instead of blocking the calling process.
   - Most relevant for **FIFOs** (named pipes):
     - Opening a FIFO for reading normally blocks until another process opens the same FIFO for writing (and vice versa).
     - With `O_NONBLOCK`, `open()` returns immediately (fails with `ENXIO` if no corresponding opener).
2. **For subsequent I/O system calls** (`read()`, `write()`, `readv()`, etc.)  
   - If the operation **cannot complete immediately**, the call either:
     - Performs a **partial transfer** (returns fewer bytes than requested), or
     - Fails immediately with `EAGAIN` or `EWOULDBLOCK` (both are synonymous on Linux and most UNIX systems).
   - The process is **not put to sleep** — it can do other work and retry later.

#### File Types Where `O_NONBLOCK` Matters
| File Type              | How `O_NONBLOCK` is Set                          | Typical Use Case |
|-----------------------|--------------------------------------------------|------------------|
| **Devices** (terminals, ptys, serial ports) | Via `open()` or `fcntl() F_SETFL`                | Prevent blocking on input/output |
| **Pipes** (anonymous) | Via `fcntl() F_SETFL` (created by `pipe()`)      | Nonblocking pipe I/O |
| **FIFOs** (named pipes) | Via `open()` (with `O_NONBLOCK` or `O_RDONLY`/`O_WRONLY`) or `fcntl()` | Prevent blocking on open/read/write |
| **Sockets**            | Via `fcntl() F_SETFL` (created by `socket()`)    | Core of asynchronous network programming |
| **Regular files**      | Usually **ignored** — kernel buffer cache makes I/O nonblocking anyway (see Chapter 13) | Exception: affects behavior with **mandatory file locking** |

#### Key Points
- `O_NONBLOCK` is a **file status flag** → can be retrieved/modified on an open descriptor using `fcntl()` `F_GETFL` / `F_SETFL`.
- For descriptors not created by `open()` (e.g., pipes from `pipe()`, sockets from `socket()`), you **must** use `fcntl()` to enable nonblocking mode.
- Errors on nonblocking I/O:
  - `EAGAIN` or `EWOULDBLOCK` → "try again later" (most common).
  - Partial reads/writes are normal and must be handled (loop until desired amount transferred).

#### Typical Pattern to Enable Nonblocking Mode
```c
// At open time (if using open())
int fd = open("fifo", O_RDWR | O_NONBLOCK);

// Or later on an existing descriptor
int flags = fcntl(fd, F_GETFL);
flags |= O_NONBLOCK;
fcntl(fd, F_SETFL, flags);
```

#### Why Use Nonblocking Mode?
- Essential for **responsive programs** (e.g., servers handling multiple clients via `poll()`, `select()`, or `epoll()`).
- Prevents a single slow/blocked I/O source from stalling the entire process.
- Foundation for **asynchronous/event-driven** programming.

## I/O on Large Files

This section explains how 32-bit systems handle files larger than 2 GB (the limit imposed by 32-bit `off_t`), and how Linux (and other UNIX systems) implement **Large File Summit (LFS)** extensions.

#### The Problem
- `off_t` (file offset type) is typically a **signed long**.
- On 32-bit systems (e.g., x86-32): 32 bits → max value = 2³¹−1 ≈ **2 GB**.
- Disk drives exceeded 2 GB long ago → need to support **large files** (> 2 GB).
- On 64-bit systems: `long` is usually 64 bits → natural support up to ~9 exabytes (though filesystem-specific limits may be lower).

#### Solutions: Two Approaches

| Approach                          | How to Enable                                      | Effect                                                                 | Status          |
|-----------------------------------|----------------------------------------------------|------------------------------------------------------------------------|-----------------|
| **Transitional LFS API** (obsolete) | Define `_LARGEFILE64_SOURCE`                       | Provides explicit 64-bit versions: `open64()`, `lseek64()`, `fseeko64()`, `stat64()`, `mmap64()`, etc.<br>Uses `off64_t` (64-bit offset type) and `struct stat64` | Obsolete — still works but not recommended |
| **Preferred: `_FILE_OFFSET_BITS=64`** | Define macro at compile time:<br>`cc -D_FILE_OFFSET_BITS=64 prog.c`<br>or `#define _FILE_OFFSET_BITS 64` in source | **Transparent**: Replaces standard functions/types with 64-bit versions.<br>`off_t` becomes 64-bit<br>`open()` → internally `open64()`<br>`lseek()` → `lseek64()`<br>etc. | **Recommended** — no source code changes needed |

#### Key Points About `_FILE_OFFSET_BITS=64`
- **No code changes required** — just recompile with the macro.
- Works only if the program is **cleanly written** (i.e., uses `off_t` for offsets/sizes, not `int` or `long` hard-coded).
- Automatically enables `O_LARGEFILE` flag in `open()` calls.
- Required glibc ≥ 2.2 and kernel ≥ 2.4 on 32-bit Linux.


#### Filesystem Support Required
- Most **native Linux filesystems** (ext3, ext4, XFS, Btrfs, etc.) support large files.
- Some **non-native** do **not**:
  - **VFAT (FAT32)** → hard 2–4 GB limit
  - **NFSv2** → hard 2 GB limit
- Even with LFS enabled, these filesystems cannot store files > 2 GB.

#### Error When Exceeding 32-bit Limits Without LFS
- Trying to access a >2 GB file with 32-bit functions → `EOVERFLOW` (e.g., from `stat()`).

#### Printing `off_t` Values (Common Pitfall)
LFS doesn’t fix `printf()` formatting.

**Wrong (if `off_t` is 64-bit)**:
```c
printf("offset = %ld\n", offset);  // may truncate on 32-bit or ILP32 systems
```

**Correct and portable**:
```c
printf("offset = %lld\n", (long long) offset);
```
- Cast to `long long` and use `%lld`.
- Same applies to `blkcnt_t` (block count in `struct stat`).

#### Module Compatibility Warning
- If passing `off_t` or `struct stat` between separately compiled objects (e.g., shared libraries), **all modules must be compiled with the same `_FILE_OFFSET_BITS` setting**.
- Otherwise → type size mismatch → crashes or corruption.

## The /dev/fd Directory

This section describes a convenient virtual directory that allows processes to refer to their own open file descriptors **as if they were filenames**.

#### `/dev/fd` – Virtual Directory of Open File Descriptors
- For **each process**, the kernel provides a special directory: **`/dev/fd`**.
- It contains entries named `/dev/fd/0`, `/dev/fd/1`, `/dev/fd/2`, ..., corresponding to the process’s open file descriptors.
  - `/dev/fd/0` → standard input (stdin)
  - `/dev/fd/1` → standard output (stdout)
  - `/dev/fd/2` → standard error (stderr)
  - Higher numbers → other open files/sockets/pipes

**Key property**:
- `open("/dev/fd/n", ...)` is **equivalent** to `dup(n)` — it returns a **new file descriptor** that refers to the **same open file description** as fd `n`.
  
Example:
```c
int fd = open("/dev/fd/1", O_WRONLY);  // Same as: int fd = dup(1);
```
- The `flags` in `open()` are interpreted (e.g., must match access mode: `O_RDONLY`, `O_WRONLY`, etc.).
- Flags like `O_CREAT`, `O_TRUNC` are **ignored** (meaningless in this context).

#### Linux-Specific Implementation
- `/dev/fd` is a **symbolic link** to **`/proc/self/fd`**.
- `/proc/self/fd` is a per-process directory under the Linux `/proc` filesystem.
- More generally, `/proc/PID/fd/` exists for **every process** with PID, containing symbolic links to all its open files (useful for debugging/monitoring).

#### Main Use Case: Shell Pipelines with Commands Expecting Filenames
Many commands expect **filename arguments**, not file descriptors or stdin/stdout directly.

**Old hack**: Use a single hyphen (`-`) to mean "use stdin/stdout".
```bash
ls | diff - oldfilelist      # diff interprets - as stdin
```
**Problems**:
- Not all programs support `-` this way.
- Some programs treat `-` as an option delimiter (e.g., `--`).
- Inconsistent and fragile.

**Better solution**: Use `/dev/fd`
```bash
ls | diff /dev/fd/0 oldfilelist   # diff gets stdin as a "filename"
```
- Works with **any** program that takes filenames.
- No special code needed in the program.

#### Convenience Symbolic Links
For readability, these standard names are provided:
- `/dev/stdin`  → symlink to `/dev/fd/0`
- `/dev/stdout` → symlink to `/dev/fd/1`
- `/dev/stderr` → symlink to `/dev/fd/2`

So you can also write:
```bash
ls | diff /dev/stdin oldfilelist
```

## Creating Temporary Files

This section describes safe, standard ways to create **temporary files** that exist only during a program's runtime and are automatically removed when no longer needed.

#### Why Temporary Files?
Many programs (e.g., compilers, editors, sort utilities) need scratch space:
- Files are created temporarily.
- Should be **unique** (no name collisions).
- Should be **secure** (no race conditions or guessing).
- Should be **removed** automatically on program termination or close.

#### 1. `mkstemp()` – Preferred Low-Level Function
```c
#include <stdlib.h>
int mkstemp(char *template);
```
- Returns: **file descriptor** on success, **–1** on error.

**How it works**:
- Caller supplies a **template** string: a pathname ending in exactly `XXXXXX` (6 X's).
- `mkstemp()` replaces the `XXXXXX` with a **unique string** (e.g., using PID + random chars).
- Creates and **opens** the file:
  - Permissions: `0600` (owner read/write only)
  - Flags: includes `O_EXCL` → guarantees exclusivity (fails if file already exists)
- Modifies the `template` string in place to contain the actual generated filename.
- `template` **must be a writable char array** (not a string literal).

**Typical safe usage pattern**:
```c
char template[] = "/tmp/myapp_XXXXXX";  // Writable array
int fd = mkstemp(template);
if (fd == -1) errExit("mkstemp");

printf("Temp file: %s\n", template);  // Optional: see the name

unlink(template);  // Critical step!

// Now use fd with read(), write(), lseek(), etc.
// File name disappears immediately from directory
// But file remains accessible via fd until close()

close(fd);  // File is finally deleted from disk
```

**Why `unlink()` immediately?**
- Removes the filename from the directory **right away** → no other process can see or access it.
- The file remains on disk (and writable) because it's still open (inode reference count > 0).
- When the last `close()` occurs → kernel deletes the file contents permanently.
- Ensures cleanup even if program crashes (as long as fd is closed normally).

#### 2. `tmpfile()` – Stdio Version
```c
#include <stdio.h>
FILE *tmpfile(void);
```
- Returns: `FILE *` stream on success, `NULL` on error.

**Behavior**:
- Creates a unique temporary file.
- Opens it in **read/write** mode (`"w+b"`).
- Uses `O_EXCL` for safety.
- **Internally calls `unlink()` immediately** → file is deleted as soon as created.
- File is automatically removed when:
  - `fclose()` is called, or
  - Program terminates normally.

**Usage**:
```c
FILE *fp = tmpfile();
if (fp == NULL) errExit("tmpfile");

// Use fprintf(), fscanf(), fread(), etc.
fprintf(fp, "Some temporary data\n");

// When done:
fclose(fp);  // File is automatically deleted
```

#### Avoid These (Insecure!)
- `tmpnam()`, `tempnam()`, `mktemp()`
  - Generate filenames **without creating the file**.
  - Vulnerable to **race conditions** (TOCTOU: another process could create the file between name generation and `open()`).
  - Can be guessed or hijacked → security holes.

#### Comparison Table

| Function      | Returns       | Interface       | Auto-unlink?                  | Secure? | Recommended? |
|---------------|---------------|-----------------|-------------------------------|---------|--------------|
| `mkstemp()`   | `int fd`      | System calls    | Manual `unlink()` needed      | Yes     | **Best for low-level** |
| `tmpfile()`   | `FILE *`      | Stdio library   | Automatic (on close/exit)     | Yes     | **Best for stdio** |
| `mktemp()` etc. | `char *` name | Name only       | No                            | No      | Avoid        |
