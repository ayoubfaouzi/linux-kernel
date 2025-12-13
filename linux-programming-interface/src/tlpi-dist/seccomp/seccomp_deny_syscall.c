/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2025.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU General Public License as published by the   *
* Free Software Foundation, either version 3 or (at your option) any      *
* later version. This program is distributed without any warranty.  See   *
* the file COPYING.gpl-v3 for details.                                    *
\*************************************************************************/

/* Supplementary program for Chapter Z */

/* seccomp_deny_syscall.c

   Usage: ./seccomp_deny_syscall <syscall-num> <command> <arg>...

   A simple seccomp filter example. Install a filter that executes
   the specified command after first installing a seccomp filter
   that will kill the program if it makes a call to the system call
   whose number is specified in the first command-line argument.
*/
#define _GNU_SOURCE
#include <stddef.h>
#include <sys/syscall.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include "tlpi_hdr.h"

/* The following is a hack to allow for systems (pre-Linux 4.14) that don't
   provide SECCOMP_RET_KILL_PROCESS, which kills (all threads in) a process.
   On those systems, define SECCOMP_RET_KILL_PROCESS as SECCOMP_RET_KILL
   (which simply kills the calling thread). */

#ifndef SECCOMP_RET_KILL_PROCESS
#define SECCOMP_RET_KILL_PROCESS SECCOMP_RET_KILL
#endif

static int
seccomp(unsigned int operation, unsigned int flags, void *args)
{
    return syscall(__NR_seccomp, operation, flags, args);
}

static void
install_filter(int syscallNum)
{
    struct sock_filter filter[] = {
        /* Architecture check deliberately omitted to keep things simple;
           a real-world application should always include the check. */

        /* Load system call number */

        BPF_STMT(BPF_LD | BPF_W | BPF_ABS,
                 offsetof(struct seccomp_data, nr)),

        /* Allow system calls other than the one with the specified number */

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, syscallNum, 1, 0),
        BPF_STMT(BPF_RET | BPF_K,           SECCOMP_RET_ALLOW),
        BPF_STMT(BPF_RET | BPF_K,           SECCOMP_RET_KILL_PROCESS)

    };

    struct sock_fprog prog = {
        .len = sizeof(filter) / sizeof(filter[0]),
        .filter = filter,
    };

    if (seccomp(SECCOMP_SET_MODE_FILTER, 0, &prog) == -1)
        errExit("seccomp");
}

int
main(int argc, char *argv[])
{
    if (argc < 3) {
        fprintf(stderr, "Usage: %s <syscall-num> <command> <arg>...\n",
                argv[0]);
    }

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
        errExit("prctl");

    install_filter(atoi(argv[1]));

    execvp(argv[2], &argv[2]);
    errExit("execve");
    exit(EXIT_SUCCESS);
}
