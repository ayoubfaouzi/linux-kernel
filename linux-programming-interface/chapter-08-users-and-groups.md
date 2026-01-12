# Users and Groups

## The Password File: /etc/passwd

Each line has **exactly seven colon-separated fields**:

```
mtk:x:1000:100:Michael Kerrisk:/home/mtk:/bin/bash
```

| Field # | Name               | Description                                                                                      | Example Value                    | Notes / Special Cases                                                                               |
| ------- | ------------------ | ------------------------------------------------------------------------------------------------ | -------------------------------- | --------------------------------------------------------------------------------------------------- |
| 1       | Login name         | Unique username (human-readable identifier)                                                      | `mtk`                            | Displayed by tools like `ls -l` instead of numeric UID                                              |
| 2       | Encrypted password | Historically: 13-char DES-encrypted password. **Modern**: Usually `x` (shadow passwords enabled) | `x` or empty or encrypted string | If empty → no password needed. If not 13 chars → login disabled (when no shadow)                    |
| 3       | User ID (UID)      | Numeric user identifier (0 = superuser/root)                                                     | `1000`                           | 0–65,535 (16-bit) on old Linux; 32-bit (much larger) on Linux 2.4+                                  |
| 4       | Group ID (GID)     | Primary group numeric ID (first group membership)                                                | `100`                            | Additional groups stored in `/etc/group`                                                            |
| 5       | Comment (GECOS)    | Free-text info about the user (e.g., full name)                                                  | `Michael Kerrisk`                | Used by `finger`, `finger(1)` shows this field                                                      |
| 6       | Home directory     | Initial working directory after login; becomes `$HOME`                                           | `/home/mtk`                      |                                                                                                     |
| 7       | Login shell        | Program started after login; becomes `$SHELL`                                                    | `/bin/bash`                      | Empty → defaults to `/bin/sh`. Can be any executable (e.g., `/usr/bin/false` for no-login accounts) |

#### ⚠️Important Modern Notes (Shadow Passwords)

- Almost all real Linux systems use **shadow passwords**:
  - `/etc/passwd` field 2 contains `x` (or any nonempty string).
  - Real encrypted password is stored in **secure** `/etc/shadow` (readable only by root).
- This separation protects encrypted passwords from being readable by ordinary users.

#### Special Cases and Behaviors

- **Multiple entries with same UID**: Allowed (uncommon).  
  → Same numeric identity → same file access rights.  
  → Different login names/passwords can be used for the same user.
- **UID 0**: Superuser privileges (usually only `root`, but possible to have others).
- **Empty shell field**: Defaults to `/bin/sh`.
- **No password required**: Empty password field → login without password (dangerous, rarely used today).

#### Networked Systems (NIS, LDAP)

- On standalone systems: all info in `/etc/passwd`.
- In networked environments (NIS, LDAP):
  - Some or all entries may be on remote servers.
  - **Transparent** to applications if they use standard library functions:
    - `getpwnam()`, `getpwuid()`, `getpwent()`, etc. (covered later in the chapter).
  - Same applies to shadow password file and `/etc/group`.

## The Shadow Password File: /etc/shadow

#### The Problem with `/etc/passwd`

- Originally, **all user information** (including encrypted passwords) was stored in `/etc/passwd`.
- Many system utilities (e.g., `ls`, `finger`) needed to read this file → it had to be **world-readable** (mode 644).
- This allowed **any user** to read the encrypted passwords.
- Attackers could:
  - Copy `/etc/passwd`.
  - Run **offline password-cracking programs** (dictionary attacks, brute force on likely passwords like names or common words).
  - Compare encrypted results against the file → crack weak passwords.

#### The Solution: Shadow Passwords (`/etc/shadow`)

- Introduced to fix this vulnerability.
- **Splits** user information:
  - **Non-sensitive data** (login name, UID, GID, home dir, shell, etc.) stays in **publicly readable** `/etc/passwd`.
  - **Encrypted password** (and additional security fields) moved to **`/etc/shadow`**, which is readable **only by root** (or privileged programs).
- Typical permissions: `/etc/shadow` is mode **640** or **600**, owned by **root:shadow** (or root:root).

#### Structure of `/etc/shadow`

Each line corresponds to one user and contains **nine colon-separated fields** (more than `/etc/passwd`):

```
mtk:$6$abc123...$def456...:19400:0:99999:7:::
```

Main fields (focus on the first two):

1. **Login name** — Matches the username in `/etc/passwd` (used as key to link records).
2. **Encrypted password** — The actual hashed password (e.g., using DES, MD5, SHA-512, bcrypt, etc.).
   - If starts with `!` or `*` → account locked/disabled.
   - If empty → no password required (rare and dangerous).
     3–9. **Additional security fields** (age, expiration, warning, etc.):
   - Last password change (days since Jan 1, 1970)
   - Minimum days before password can be changed
   - Maximum password age
   - Warning period before expiration
   - Inactivity period before account disabled
   - Account expiration date
   - Reserved field

(Full details in `shadow(5)` man page.)

#### How Shadow Passwords Work in Practice

- When a user logs in:
  - Login program (e.g., `login`, `sshd`) reads `/etc/passwd` for basic info.
  - Reads `/etc/shadow` (as root/privileged) to verify the password.
- Normal users cannot read `/etc/shadow` → cracking programs cannot get the encrypted passwords easily.

#### Important Notes

- **Not standardized**:
  - **SUSv3 / POSIX** does **not** specify shadow passwords.
  - Feature is **not universal** across all UNIX systems.
  - Common on Linux, many BSDs, Solaris, etc., but some older/minimal systems may not use it.
- Most modern Linux distributions **enable shadow passwords by default**.
- The transition is transparent to applications that use standard library functions (`getpwnam()`, `getpwuid()`, etc.).

## The Group File: /etc/group

- Groups allow multiple users to share access to files, directories, and other resources.
- A user’s **complete set of group memberships** comes from **two places** (historical design):
  1. The **primary group** → specified by the **GID** field in the user’s `/etc/passwd` entry.
  2. **Supplementary (secondary) groups** → listed in `/etc/group`.

This split is a legacy from early UNIX:

- Originally: users belonged to **only one group** at a time (set by GID in `/etc/passwd`).
- Changed via `newgrp(1)` (which sometimes required a group password).
- **4.2BSD** (1983) introduced **multiple simultaneous group memberships** → later standardized in POSIX.1-1990.

Today: users typically have **one primary group** + **zero or more supplementary groups**.

#### Viewing Current Groups

- `groups` command (without args) → shows groups of the current process/shell.
- `groups username` → shows groups for specified user(s).

#### The Group File: `/etc/group`

Each line defines one group and has **exactly four colon-separated fields**:

**Example lines**:

```
users:x:100:
jambit:x:106:claus,felli,frank,harti,markus,martin,mtk,paul
```

| Field # | Name               | Description                                                                       | Example Value           | Notes / Special Cases                                                                 |
| ------- | ------------------ | --------------------------------------------------------------------------------- | ----------------------- | ------------------------------------------------------------------------------------- |
| 1       | Group name         | Unique symbolic name for the group (human-readable)                               | `users`, `jambit`       | Used by tools like `ls -l` (shows group name instead of numeric GID)                  |
| 2       | Encrypted password | Historically: optional group password (rarely used today)                         | `x` (or empty)          | With **shadow groups** enabled → conventionally `x`; real password in `/etc/gshadow`  |
| 3       | Group ID (GID)     | Numeric identifier for the group                                                  | `100`, `106`            | 0–65,535 (16-bit) on old Linux; 32-bit on Linux 2.4+                                  |
| 4       | User list          | Comma-separated list of usernames who are **supplementary** members of this group | `claus,felli,frank,...` | Does **not** include users whose primary group is this one (they’re in `/etc/passwd`) |

#### How Group Membership Is Determined (Example)

User `avr` has:

- Primary group: **users** (GID 100 from `/etc/passwd`):
  ```
  avr:x:1001:100:Anthony Robins:/home/avr:/bin/bash
  ```
- Supplementary groups: **staff** and **teach** (listed in `/etc/group`):
  ```
  staff:x:101:mtk,avr,martinl
  teach:x:104:avr,rlb,alc
  ```

→ Final effective groups for `avr`: **users**, **staff**, **teach**.

#### Key Points About Group Passwords

- **Rarely used** today (multiple memberships made them obsolete).
- If present and shadowing enabled → field is `x` (real password in `/etc/gshadow`).
- `newgrp groupname` → switches primary group of current shell (asks for group password if set and user not a member).

#### Historical vs. Modern Design

- **Primary GID** (in `/etc/passwd`) is still required — defines the **default group** for new files created by the user.
- **Supplementary groups** (in `/etc/group`) provide additional access rights without changing the primary group.

## Retrieving User and Group Information

### 1. Retrieving Individual Records from the Password File (`/etc/passwd`)

```c
#include <pwd.h>

struct passwd *getpwnam(const char *name);   // Lookup by username
struct passwd *getpwuid(uid_t uid);          // Lookup by numeric UID
```

- Both return a pointer to a statically allocated `struct passwd` on success, or `NULL` if not found or error.
- Structure fields:

  ```c
  struct passwd {
      char   *pw_name;    /* Username (login name) */
      char   *pw_passwd;  /* Encrypted password (only if no shadow) */
      uid_t   pw_uid;     /* User ID */
      gid_t   pw_gid;     /* Primary group ID */
      char   *pw_gecos;   /* Comment / GECOS field (e.g., full name) */
      char   *pw_dir;     /* Home directory */
      char   *pw_shell;   /* Login shell */
  };
  ```

- **Important notes**:
  - `pw_passwd` is **not useful** if shadow passwords are enabled (most modern systems).
  - `pw_gecos` and `pw_passwd` are **not in SUSv3** but exist on virtually all UNIX implementations.
  - Return value points to **static memory** → overwritten on next call to any of these functions (or `getpwent()`).
  - **Not reentrant** — unsafe in multithreaded programs or with signals.
  - **Reentrant alternatives** (SUSv3): `getpwnam_r()`, `getpwuid_r()` — require caller-supplied buffer.

- **Error vs. Not Found**:
  - SUSv3: `NULL` + `errno` unchanged → "not found".
  - Real-world (including old glibc): often sets `errno` (e.g., `ENOENT`, `ESRCH`) → hard to distinguish portably.
  - Modern glibc (≥2.7): conforms to SUSv3 → `errno` unchanged on "not found".

### 2. Retrieving Individual Records from the Group File (`/etc/group`)

```c
#include <grp.h>

struct group *getgrnam(const char *name);    // By group name
struct group *getgrgid(gid_t gid);           // By numeric GID
```

- Return `struct group *` (static, overwritten on next call).
- Structure fields:

  ```c
  struct group {
      char   *gr_name;    /* Group name */
      char   *gr_passwd;  /* Encrypted group password (rarely used, usually x) */
      gid_t   gr_gid;     /* Group ID */
      char  **gr_mem;     /* NULL-terminated array of usernames (supplementary members) */
  };
  ```

- Same caveats as password functions: static, not reentrant, `gr_passwd` ignored with shadow groups.

### 3. Scanning All Records (Sequential Read)

**Password file**:

```c
#include <pwd.h>

struct passwd *getpwent(void);   // Get next record
void setpwent(void);             // Rewind to start
void endpwent(void);             // Close file
```

- `getpwent()`: Returns next record (opens file automatically on first call).
- Returns `NULL` at end or error.
- `endpwent()`: **Must** be called to close file (allows future scans to restart).
- `setpwent()`: Rewinds to beginning without closing.

**Group file** equivalents: `getgrent()`, `setgrent()`, `endgrent()`.

**Example** (scan all users):

```c
struct passwd *pwd;
while ((pwd = getpwent()) != NULL)
    printf("%-8s  %5ld\n", pwd->pw_name, (long)pwd->pw_uid);
endpwent();
```

### 4. Shadow Password File Functions (`/etc/shadow`)

```c
#include <shadow.h>

struct spwd *getspnam(const char *name);   // By username
struct spwd *getspent(void);               // Sequential scan
void setspent(void);
void endspent(void);
```

- Return `struct spwd *` (static, like above).
- Structure fields (focus on password aging):

  ```c
  struct spwd {
      char *sp_namp;         /* Username */
      char *sp_pwdp;         /* Encrypted password */
      long  sp_lstchg;       /* Last change (days since 1970-01-01) */
      long  sp_min;          /* Min days between changes */
      long  sp_max;          /* Max days before must change */
      long  sp_warn;         /* Warning days before expiration */
      long  sp_inact;        /* Days inactive before account locked */
      long  sp_expire;       /* Account expiration date */
      unsigned long sp_flag; /* Reserved */
  };
  ```

- **Not in SUSv3** → availability varies (common on Linux).
- Used mainly to check password status/aging.

## Password Encryption and User Authentication

- Applications sometimes need to authenticate users using the **system's real username/password** (from `/etc/passwd` + `/etc/shadow`).
- **Never** read the encrypted password directly and compare it — use the **standard mechanism** instead.
- Passwords are stored using **one-way encryption** (irreversible) → you can only **verify** by encrypting the candidate password and comparing the result.

#### The `crypt()` Function – Core Password Encryption

```c
#define _XOPEN_SOURCE
#include <unistd.h>

char *crypt(const char *key, const char *salt);
```

- **key**: The plaintext password (up to 8 characters historically; modern algorithms handle longer).
- **salt**: 2-character string that perturbs the algorithm (makes precomputed attacks harder).
- Returns: Pointer to a **statically allocated** 13-character string (encrypted password) or `NULL` on error.

**How it works**:

- Historically used **DES** (Data Encryption Standard) → 13-char result.
- Modern systems use stronger algorithms (MD5, SHA-512, bcrypt, etc.):
  - Result is longer (e.g., 34 chars for MD5, starts with `$`).
  - First few characters identify the algorithm (e.g., `$6$` for SHA-512).
- The **salt** is included in the returned string → you can pass the **existing encrypted password** as the salt when verifying!

**Verification pattern** (most common use):

```c
char *encrypted = get_encrypted_password_from_shadow(username);  // e.g., from getspnam()
char *candidate = get_user_input_password();

if (strcmp(crypt(candidate, encrypted), encrypted) == 0) {
    // Password matches!
}
```

- `crypt()` ignores anything after the first 2 chars of `salt`, so passing the full encrypted string works perfectly.

**Compilation note**:

- On Linux: Link with `-lcrypt` (separate crypt library).

#### The `getpass()` Function – Safe Password Input

```c
#define _BSD_SOURCE
#include <unistd.h>

char *getpass(const char *prompt);
```

- Prints `prompt` (e.g., "Password: ").
- Disables terminal **echo** and special character processing (e.g., Ctrl-C).
- Reads one line of input (password).
- Returns pointer to **statically allocated**, null-terminated string (newline stripped).
- Restores terminal settings afterward.

**Important**:

- Static buffer → overwritten on next call.
- **Not in SUSv3** (marked LEGACY in SUSv2) → not portable, but widely available.
- Modern code often implements this manually using `termios` (Chapter 62).

#### Security Considerations

- Passwords are **never stored in plaintext**.
- Even privileged processes can't recover the original password.
- Risks remain:
  - Swap file leakage (if page containing password is swapped out).
  - `/dev/mem` reading by privileged attacker.
- These are mitigated by modern systems (e.g., memory locking, secure kernels).
