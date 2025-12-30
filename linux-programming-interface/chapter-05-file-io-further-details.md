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

**Correct solution**: Use `O_CREAT | O_EXCL`

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

**Correct solution**: Open the file with `O_APPEND`

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
