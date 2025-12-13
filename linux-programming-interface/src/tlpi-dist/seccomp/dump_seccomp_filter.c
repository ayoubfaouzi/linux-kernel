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

/* dump_seccomp_filter.c

   Dump the contents of a process's BPF filter to a file, using the
   PTRACE_SECCOMP_GET_FILTER operation described in ptrace(2).

   Usage: dump_seccomp_filter PID dump-file [filter-index]

   A filter index of 0 (the default if no argument is supplied) corresponds
   to the most recently installed seccomp filter, with increasing values
   corresponding to previously installed filters. An ENOENT error return
   indicates that the index value exceeds the number of filters installed
   for the process.

   The dumped filter can be disassembled using a tool such as libseccomp's
   disassembler:

       libseccomp/tools/scmp_bpf_disasm < dump-file
*/
#include <linux/seccomp.h>
#include <linux/filter.h>
#include <sys/ptrace.h>
#include <sys/wait.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>
#include <fcntl.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); } while (0)

/* Fetch BPF filter with specified index for specified PID. A pointer to a
   dynamically allocated buffer is containing the filter code returned as the
   function result; the caller is responsible for freeing this buffer. If
   'instrCnt' is not NULL, then the number of instructions in the filter is
   placed in *instrCnt. */

static struct sock_filter *
fetchFilter(pid_t pid, int filterIndex, int *instrCnt, bool quiet)
{
    /* Attach to the target process and wait for it to be stopped by
       the attach operation */

    if (ptrace(PTRACE_ATTACH, pid, 0, 0) == -1) {
        /* E.g., attempting to attach to a kernel thread gives EPERM */
        if (!quiet)
            fprintf(stderr, "%ld: could not PTRACE_ATTACH (%s)\n", (long) pid,
                    strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (waitpid(pid, NULL, 0) == -1)
        errExit("waitpid");

    /* Discover the number of instructions in the BPF filter */

    int icnt = ptrace(PTRACE_SECCOMP_GET_FILTER, pid, filterIndex, NULL);
    if (icnt == -1) {
        if (errno == EINVAL) {
            if (!quiet)
                fprintf(stderr, "%ld: does not have any BPF filters\n",
                        (long) pid);
            exit(EXIT_FAILURE);
        } else if (errno == ENOENT) {
            if (!quiet)
                fprintf(stderr, "%ld: no BPF program exists at index %d\n",
                        (long) pid, filterIndex);
            exit(EXIT_FAILURE);
        } else if (errno == EACCES) {   /* As documented in ptrace(2)... */
            fprintf(stderr, "You lack the CAP_SYS_ADMIN capability; "
                    "run this program as root\n");
            exit(EXIT_FAILURE);
        } else {
            errExit("ptrace - PTRACE_SECCOMP_GET_FILTER-1");
        }
    }

    /* Allocate a buffer and fetch the content of the BPF filter */

    struct sock_filter *filterProg;
    filterProg = calloc(icnt, sizeof(struct sock_filter));
    if (filterProg == NULL)
        errExit("calloc");

    icnt = ptrace(PTRACE_SECCOMP_GET_FILTER, pid, filterIndex, filterProg);
    if (icnt == -1)
        errExit("ptrace - PTRACE_SECCOMP_GET_FILTER-2");

    if (instrCnt != NULL)
        *instrCnt = icnt;

    return filterProg;
}

/* Dump filter contents to a file */

static void
dumpFilter(char *pathname, struct sock_filter *filterProg, int instrCnt)
{
    int fd = open(pathname, O_CREAT | O_TRUNC | O_WRONLY, S_IRUSR | S_IWUSR);
    if (fd == -1)
        errExit("open");

    if (write(fd, filterProg, instrCnt * sizeof(struct sock_filter)) == -1)
        errExit("write");

    if (close(fd) == -1)
        errExit("close");
}

static void
usageError(char *pname, char *msg)
{
    fprintf(stderr, "%s", msg);
    fprintf(stderr, "Usage: %s [-q] PID dump-file [filter-index]\n", pname);
    fprintf(stderr, "       <filter-index> defaults to 0 (the most recently "
                    "installed filter)\n");
    fprintf(stderr, "       -q    Quiet mode; don't print messages on success "
                    "or on expected errors\n");
    exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
    bool quiet = false;
            /* If set true, don't produce output on success or for expected
               errors. In this case, the exit status tells us whether or not a
               BPF filter was dumped. */
    int opt;
    while ((opt = getopt(argc, argv, "q")) != -1) {
        switch (opt) {
        case 'q':
            quiet = true;
            break;
        default:
            usageError(argv[0], "Bad option\n");
        }
    }

    if (optind + 2 > argc)
        usageError(argv[0], "Missing arguments\n");

    pid_t pid = atoi(argv[optind]);
    int filterIndex = (argc > optind + 2) ? atoi(argv[optind + 2]) : 0;

    int instrCnt;       /* Number of instructions in BPF filter */
    struct sock_filter *filterProg = fetchFilter(pid, filterIndex, &instrCnt,
                                                 quiet);

    dumpFilter(argv[optind + 1], filterProg, instrCnt);
    free(filterProg);

    if (!quiet)
        printf("%ld: dumped %d BPF instructions from filter %d\n", (long) pid,
                instrCnt, filterIndex);

    exit(EXIT_SUCCESS);
}
