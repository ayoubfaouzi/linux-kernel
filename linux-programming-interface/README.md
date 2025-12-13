# Linux Programming Interface

## Preface

The author pays special attention to portability, explaining behaviors across different UNIX systems (`Solaris`, `FreeBSD`, `macOS`, `Tru64`, `HP-UX`) and clearly marking Linux-specific features.

The book targets Linux `kernel 2.6.x` and `glibc 2.x`, with differences from Linux 2.4 noted where relevant.

The use of **nonstandard extensions** is sometimes essential, either for performance reasons or to access functionality that is unavailable in the standard UNIX programming interface.

Therefore, while I’ve designed this book to be useful to programmers working with all UNIX implementations, I also provide full coverage of programming features that are **specific to Linux**. These features include:
- `epoll`, a mechanism for obtaining notification of file I/O events;
- `inotify`, a mechanism for monitoring changes in files and directories;
- **capabilities**, a mechanism for granting a process a subset of the powers of the superuser;
- **extended attributes**;
- **i-node** flags;
- the `clone()` system call;
- the `/proc` file system; and
- Linux-specific details of the implementation of file I/O, signals, timers, threads, shared libraries, inter-process communication, and sockets.


## References

- Official website: https://man7.org/tlpi/index.html
