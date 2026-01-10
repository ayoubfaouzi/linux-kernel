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
