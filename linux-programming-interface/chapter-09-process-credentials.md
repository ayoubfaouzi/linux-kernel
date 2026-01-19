# Process Credentials

Every process has a set of associated numeric user identifiers (UIDs) and group identifiers (GIDs). Sometimes, these are referred to as process credentials. These identifiers are as follows:

- real user ID and group ID;
- effective user ID and group ID;
- saved set-user-ID and saved set-group-ID;
- file-system user ID and group ID (Linux-specific);
- supplementary group IDs.

## Real User ID and Real Group ID

- Represent the **true identity** of the user/group running the process.
- Set during login: login shell gets them from fields 3 and 4 of the user's `/etc/passwd` entry.
- Inherited by all child processes (via `fork()`).
- Normally unchanging throughout the process lifetime (except by `root`).
- Used mainly for **accounting** (e.g., _who owns the process_) and some security checks.

## Effective User ID and Effective Group ID

- On **most UNIX systems** (and Linux, with minor differences noted in Section 9.5):
  - The **effective UID**, **effective GID**, and **supplementary group IDs** together determine the **permissions** granted to a **process** when it performs operations.
- Typical uses:
  - **File access** (read/write/execute permissions checked against file owner/group and process effective/supplementary IDs).
  - **System V IPC objects** (shared memory, semaphores, message queues) — ownership checked similarly.
  - **Sending signals** (Section 20.5): a process can signal another only if effective UID matches real/effective UID of target (or is root).
  - Many other system calls (e.g., binding privileged ports <1024, changing ownership, etc.).
- A process with **effective UID == 0** (root) is **privileged** (superuser).
  - Can bypass most permission checks.
  - Can perform restricted system calls (e.g., `setuid()`, `mount()`, `kill()` any process).
- Linux later adds **capabilities** (Chapter 39): splits root privileges into finer-grained units (e.g., `CAP_SYS_ADMIN`, `CAP_KILL`) that can be enabled/disabled independently.
- By default, **effective UID/GID == real UID/GID**.
  - 📌 Process runs with the privileges of the actual user who started it.
- Effective IDs can be changed in **two main ways**:
  1. **Explicitly via system calls** (covered in Section 9.7):
  - `seteuid()`, `setegid()`, `setreuid()`, `setregid()`, etc.
  - Allows temporary privilege changes.
  2. **Automatically via set-user-ID and set-group-ID programs** (covered later):
  - When executing a program with the **set-user-ID** bit set → effective UID becomes the **owner** of the executable file.
  - Similarly for set-group-ID → effective GID becomes the **group** of the file.
  - This is the foundation for programs like `passwd`, `sudo`, `ping` that need temporary elevated privileges.

## Set-User-ID and Set-Group-ID Programs

- A **set-user-ID program**:
  - When executed, sets the process's **effective UID** to the **owner** (user ID) of the executable file.
- A **set-group-ID program**:
  - Sets the process's **effective GID** to the **group** owner of the file.
- This gives the process **privileges it wouldn't normally have** (e.g., root privileges if owned by root).
- Every file has two special permission bits: **set-user-ID** and **set-group-ID**.
- Set using `chmod`:
  ```bash
  chmod u+s prog    # Set-user-ID bit
  chmod g+s prog    # Set-group-ID bit
  ```
- When displayed with `ls -l`, the execute bit `x` becomes `s`:
    ```bash
    -rwsr-sr-x  1 root root 302585 Jun 26 15:05 prog
    ```
  - `s` in owner execute position → set-user-ID
  - `s` in group execute position → set-group-ID
- When a set-user-ID program runs:
  - **Effective UID** = **owner UID** of the file (not the real user).
  - Real UID stays the same (the actual user who ran it).
- Same for set-group-ID → effective GID = file's group owner.
- 👉 Process gains the **privileges** of the file owner/group for the duration of the program.

#### Common Real-World Examples

| Program     | Set-ID Type  | Owner | Purpose                                                                      |
| ----------- | ------------ | ----- | ---------------------------------------------------------------------------- |
| `passwd(1)` | set-user-ID  | root  | Allows ordinary users to change their own password (writes to `/etc/shadow`) |
| `mount(8)`  | set-user-ID  | root  | Allows mounting filesystems (privileged operation)                           |
| `su(1)`     | set-user-ID  | root  | Switch user (needs root privileges)                                          |
| `wall(1)`   | set-group-ID | tty   | Write message to all terminals (access to tty devices)                       |

#### Special Cases and Terminology

- **set-user-ID-root program**: Owned by root with set-user-ID bit → process gains **full superuser privileges** (effective UID = 0).
- **set-user-ID to non-root user**: Gains privileges of that specific user (not root) — useful for restricted access to a resource.
- **Both bits set**: Possible (uncommon) — process gets both effective UID and GID from file owner/group.

#### Example from the Book

To allow any user to run the password-checking program (Listing 8-2) that needs `/etc/shadow` access:

```bash
# chown root check_password     # Make it owned by root
# chmod u+s check_password      # Enable set-user-ID bit
# ls -l check_password
-rwsr-xr-x 1 root users 18150 Oct 28 10:49 check_password
```

Now ordinary users can run it and authenticate against the shadow file.

- Extremely powerful: Lets unprivileged users perform privileged tasks safely.
- Extremely dangerous if the program is poorly written → security vulnerabilities.

## Saved Set-User-ID and Saved Set-Group-ID

#### When and How Saved IDs Are Set

During every `exec()` (program execution), the kernel performs (among other steps):

1. **If the file has the set-user-ID bit set**:
   - Effective UID ← **owner UID** of the executable file.
   - Otherwise, effective UID remains unchanged.
   - (Same logic applies to set-group-ID → effective GID ← file's group owner.)
2. **Regardless of the set-ID bits**:
   - **Saved set-user-ID** ← current **effective UID** (after step 1).
   - **Saved set-group-ID** ← current **effective GID** (after step 1).

This happens **every time** a program is executed — even for normal (non-set-ID) programs.

#### Example: Running a Set-User-ID-Root Program

- Initial process state (before `exec()`):
  - Real UID = 1000
  - Effective UID = 1000
  - Saved set-UID = 1000
- Executes a **set-user-ID program owned by root** (UID 0):
- After `exec()`:
  - **Real UID** = 1000 (unchanged)
  - **Effective UID** = 0 (because set-user-ID bit was set → file owner UID)
  - **Saved set-UID** = 0 (copied from the new effective UID)
👉 Process now runs with **full superuser privileges** (effective UID = 0), but the **saved set-UID remembers** that it can return to 0 later.

#### Purpose of Saved IDs: Temporary Privilege Dropping

- The saved IDs allow a program to:
  - **Start with elevated privileges** (effective = file owner, e.g., root).
  - **Temporarily drop privileges** (set effective UID back to real UID = 1000 → become unprivileged).
  - **Later regain privileges** (set effective UID back to **saved set-UID** = 0).
- 👍 This is **essential for secure programming**:
  - A set-user-ID-root program should **not** run as root the entire time.
  - It should drop to the real (unprivileged) user whenever possible.
  - Use saved set-UID to **regain root** only when needed.

#### How to Change Effective IDs (Preview)

Various system calls allow switching:

- `seteuid()`, `setegid()` → change only effective ID.
- `setreuid()`, `setregid()` → change real and/or effective (with restrictions).
- A privileged process can set effective to **saved** or **real** UID/GID.

## File-System User ID and File-System Group ID

- On **most UNIX systems** (and historically on Linux too):
  - **Effective UID** and **effective GID** (plus supplementary groups) determine:
    - File access permissions (open, read, write, chmod, chown, etc.)
    - Creation of new files (ownership)
    - Other privileged operations
- On **Linux** (since kernel 1.2):
  - File-system operations (opening files, chmod, chown, etc.) use the **file-system UID/FSGID** instead of the effective UID/GID.
  - All **other** privilege checks (signals, binding low ports, etc.) still use the **effective** UID/GID.

#### Default Behavior
- Normally, **FSUID = effective UID** and **FSGID = effective GID** (and thus usually = real IDs too).
- Whenever the **effective** UID or GID changes (via system call or exec of set-user-ID/set-group-ID program), the **file-system** IDs are **automatically updated** to match.
- 👉 In almost all practical cases, Linux behaves **exactly like other UNIX implementations** — file permissions are checked using effective IDs 🤷‍♀️.

#### When and Why They Can Differ
- The only way FSUID/FSGID differ from effective UID/GID is if you **explicitly** set them using the **Linux-specific** system calls:
  ```c
  int setfsuid(uid_t fsuid);
  int setfsgid(gid_t fsgid);
  ```
- These calls are **rarely used** in modern code.

#### Historical Reason for File-System IDs
- Introduced in **Linux 1.2** (early 1990s) to solve a problem with the **Linux NFS server**.
- Old signal rules (pre-2.0): A process could send a signal to another if sender's **effective UID** matched target's **real** or **effective** UID.
- NFS server needed to:
  - Access files **as if** it had the client's effective UID/GID → needed to change its own IDs.
  - But if it changed effective UID, it became **vulnerable** to signals from unprivileged users.
- 👉 Use **separate** file-system IDs for file access, while keeping **effective** IDs unchanged → safe from signals.

#### Modern Status
- From **kernel 2.0** onward, Linux adopted **POSIX/SUSv3** signal rules (Section 20.5):
  - Signal permission no longer depends on effective UID of target.
  - File-system IDs are **no longer strictly necessary**.
- Today, the same result can be achieved by **temporarily changing effective** UID/GID (drop to unprivileged, do file operation, regain via saved IDs).
- File-system IDs remain **for backward compatibility** with old software (especially NFS-related).

#### Practical Impact
- In virtually all modern programs, **FSUID/FSGID == effective UID/GID**.
- Therefore, the book (and most documentation) describes file permission checks in terms of **effective IDs** — the presence of file-system IDs **seldom makes a difference** on current Linux.
- You can generally **ignore** FSUID/FSGID unless:
  - You're writing/maintaining very old code.
  - You're dealing with legacy NFS servers.
  - You're using `setfsuid()`/`setfsgid()` explicitly (very rare).

## Supplementary Group IDs

- Supplementary group IDs are a **set** (list) of **additional numeric group IDs** (GIDs) that a process is a member of.
- They are **in addition to** the process's **primary group** (defined by the GID field in `/etc/passwd`).
  - Used **together with** the **effective UID/GID** (and on Linux, file-system UID/GID) to determine **access permissions** for:
    - Files and directories (read/write/execute)
    - System V IPC objects (shared memory, semaphores, message queues)
    - Other system resources that use group-based authorization
- A **new process** (created via `fork()`) **inherits** its **complete set** of supplementary group IDs from its parent.
- The **login shell** obtains its supplementary groups from the **system group file** (`/etc/group`):
  - All entries where the user's login name appears in the **member list** (fourth field).

#### How Supplementary Groups Are Determined (Example)

User `ayoub` has primary group `users` (GID 100 from `/etc/passwd`):
```sh
ayoub:x:1001:100:Ayoub:/home/ayoub:/bin/bash
```

And belongs to supplementary groups `staff` and `developers` (listed in `/etc/group`):
```sh
staff:x:101:ayoub,martin
developers:x:105:ayoub,teamlead
```

→ Process credentials for `ayoub`'s shell/login:
  - Real UID: 1001
  - Effective UID: 1001 (unless setuid program)
  - Primary GID: 100 (`users`)
  - Supplementary GIDs: 101 (`staff`), 105 (`developers`)
→ `ayoub` gets file access rights from **all four groups**: `users`, `staff`, `developers`, and his primary group.

## Retrieving and Modifying Process Credentials
