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

/* seccomp_unotify_openat.c

   Demonstrate the SECCOMP_IOCTL_NOTIF_ADDFD ioctl(2) added in Linux 5.9.
   This operation allows a user-space notification supervisor to open
   a file on behalf of the target, and pass that file descriptor back
   as the return value of the target's system call.
*/
#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <poll.h>
#include <signal.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include "scm_functions.h"
#include "seccomp_functions.h"
#include "tlpi_hdr.h"

#ifndef SECCOMP_IOCTL_NOTIF_ADDFD

#define SECCOMP_IOCTL_NOTIF_ADDFD   SECCOMP_IOW(3, struct seccomp_notif_addfd)
    /* On success, the return value is the remote process's added fd number */

/* valid flags for seccomp_notif_addfd */
#define SECCOMP_ADDFD_FLAG_SETFD        (1UL << 0) /* Specify remote fd */

struct seccomp_notif_addfd {
    __u64 id;           /* Cookie from SECCOMP_IOCTL_NOTIF_RECV */
    __u32 flags;        /* Flags:
                           SECCOMP_ADDFD_FLAG_SETFD: allow target FD
                           to be selected via 'newfd' field. */
    __u32 srcfd;        /* FD to duplicate in supervisor */
    __u32 newfd;        /* 0, or desired FD number in target */
    __u32 newfd_flags;  /* Flags to set om target FD (O_CLOEXEC) */
};

#define SECCOMP_IOC_MAGIC               '!'
#define SECCOMP_IO(nr)                  _IO(SECCOMP_IOC_MAGIC, nr)
#define SECCOMP_IOR(nr, type)           _IOR(SECCOMP_IOC_MAGIC, nr, type)
#define SECCOMP_IOW(nr, type)           _IOW(SECCOMP_IOC_MAGIC, nr, type)
#define SECCOMP_IOWR(nr, type)          _IOWR(SECCOMP_IOC_MAGIC, nr, type)
#endif

static void
sigchldHandler(int sig)
{
    char msg[] = "\tS: target has terminated; bye\n";

    write(STDOUT_FILENO, msg, sizeof(msg) - 1);
    _exit(EXIT_SUCCESS);
}

/* The following is the x86-64-specific BPF boilerplate code for checking that
   the BPF program is running on the right architecture + ABI. At completion
   of these instructions, the accumulator contains the system call number. */

/* For the x32 ABI, all system call numbers have bit 30 set */

#define X32_SYSCALL_BIT         0x40000000

#define X86_64_CHECK_ARCH_AND_LOAD_SYSCALL_NR \
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, \
                offsetof(struct seccomp_data, arch)), \
        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, AUDIT_ARCH_X86_64, 0, 2), \
        BPF_STMT(BPF_LD | BPF_W | BPF_ABS, \
                 offsetof(struct seccomp_data, nr)), \
        BPF_JUMP(BPF_JMP | BPF_JGE | BPF_K, X32_SYSCALL_BIT, 0, 1), \
        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_KILL_PROCESS)

/* installNotifyFilter() installs a seccomp filter that generates
   user-space notifications (SECCOMP_RET_USER_NOTIF) when the process
   calls openat(2); the filter allows all other system calls.

   The function return value is a file descriptor from which the
   user-space notifications can be fetched. */

static int
installNotifyFilter(void)
{
    struct sock_filter filter[] = {
        X86_64_CHECK_ARCH_AND_LOAD_SYSCALL_NR,

        /* openat() triggers notification to user-space supervisor */

        BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, __NR_openat, 0, 1),
        BPF_STMT(BPF_RET + BPF_K, SECCOMP_RET_USER_NOTIF),

        /* Every other system call is allowed */

        BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW),
    };

    struct sock_fprog prog = {
        .len = sizeof(filter) / sizeof(filter[0]),
        .filter = filter,
    };

    /* Install the filter with the SECCOMP_FILTER_FLAG_NEW_LISTENER flag;
       as a result, seccomp() returns a notification file descriptor. */

    /* Only one listening file descriptor can be established. An attempt to
       establish a second listener yields an EBUSY error. */

    /* Because of conflicting uses of the system call return value,
       'flags' can't contain both SECCOMP_FILTER_FLAG_NEW_LISTENER
       and SECCOMP_FILTER_FLAG_TSYNC (or an EINVAL error results). */

    int notifyFd = seccomp(SECCOMP_SET_MODE_FILTER,
                           SECCOMP_FILTER_FLAG_NEW_LISTENER, &prog);
    if (notifyFd == -1)
        errExit("seccomp-install-notify-filter");

    return notifyFd;
}

/* Close a pair of sockets created by socketpair() */

static void
closeSocketPair(int sockPair[2])
{
    if (close(sockPair[0]) == -1)
        errExit("closeSocketPair-close-0");
    if (close(sockPair[1]) == -1)
        errExit("closeSocketPair-close-1");
}

/* Implementation of the target process; create a child process that:

   (1) installs a seccomp filter with the
       SECCOMP_FILTER_FLAG_NEW_LISTENER flag;
   (2) writes the seccomp notification file descriptor returned from
       the previous step onto the UNIX domain socket, 'sockPair[0]';
   (3) calls openat(2) for each element of 'argv'.

   The function return value in the parent is the PID of the child
   process; the child does not return from this function. */

static pid_t
targetProcess(int sockPair[2], char *argv[])
{
    pid_t targetPid = fork();
    if (targetPid == -1)
        errExit("fork");

    if (targetPid > 0)          /* In parent, return PID of child */
        return targetPid;

    /* Child falls through to here */

    printf("T: PID = %ld\n", (long) getpid());

    /* Install seccomp filter */

    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
        errExit("prctl");

    int notifyFd = installNotifyFilter();

    /* Pass the notification file descriptor to the supervisor process over
       a UNIX domain socket */

    if (sendfd(sockPair[0], notifyFd) == -1)
        errExit("sendfd");

    /* Notification and socket FDs are no longer needed in target process */

    if (close(notifyFd) == -1)
        errExit("close-target-notify-fd");

    closeSocketPair(sockPair);

    /* Perform a openat() call for each of the command-line arguments */

    for (char **ap = argv; *ap != NULL; ap++) {
        printf("\nT: about to openat(\"%s\")\n", *ap);

        int fd = openat(AT_FDCWD, *ap, O_CREAT | O_RDWR, 0600);
        if (fd == -1) {
            perror("T: ERROR: openat(2)");
            continue;
        }
        printf("T: SUCCESS: openat(2) returned %d\n", fd);

        ssize_t nr;
        char buf[100];
        while ((nr = read(fd, buf, sizeof(buf))) > 0)
            write(STDOUT_FILENO, buf, nr);
        write(STDOUT_FILENO, "==========\n", 11);

        close(fd);
    }

    printf("\nT: terminating\n");
    exit(EXIT_SUCCESS);
}

/* Handle notifications that arrive via the SECCOMP_RET_USER_NOTIF file
   descriptor, 'notifyFd'. */

static void
handleNotifications(int notifyFd)
{
    struct seccomp_notif_sizes sizes;
    struct seccomp_notif *req;
    struct seccomp_notif_resp *resp;
    char path[PATH_MAX];

    allocSeccompNotifBuffers(&req, &resp, &sizes);

    /* Loop handling notifications */

    for (;;) {

        /* Wait for next notification, returning info in '*req' */

        memset(req, 0, sizes.seccomp_notif);    /* Required since Linux 5.5 */
        if (ioctl(notifyFd, SECCOMP_IOCTL_NOTIF_RECV, req) == -1) {
            if (errno == EINTR)
                    continue;

            errExit("ioctl-SECCOMP_IOCTL_NOTIF_RECV");
        }

        printf("\tS: got notification (ID %#llx) for PID %d\n",
                req->id, req->pid);

        /* The only system call that can generate a notification event
           is openat(2). Nevertheless, we check that the notified system
           call is indeed openat() as a kind of future-proofing of this
           code in case the seccomp filter is later modified to
           generate notifications for other system calls. */

        if (req->data.nr != __NR_openat) {
            printf("\tS: notification contained unexpected "
                    "system call number; bye!!!\n");
            exit(EXIT_FAILURE);
        }

        int pathStatus = getTargetPathname(req, notifyFd, 1, path,
                                        sizeof(path));

        /* Prepopulate some fields of the response */

        resp->id = req->id;     /* Response includes notification ID */
        resp->flags = 0;
        resp->val = 0;

        /* If the target pathname was not valid, trigger an EINVAL error;
           otherwise execute an openat() call and pass the resulting file
           descriptor to the target. */

        if (pathStatus != 0) {

            resp->error = -EINVAL;
            printf("\tS: spoofing error for invalid pathname (%s)\n",
                    strerror(-resp->error));

        } else {

            printf("\tS: executing: openat(%#llo, \"%s\", %#llo, %#llo)\n",
                    req->data.args[0], path, req->data.args[2],
                    req->data.args[3]);

            int fd = openat(req->data.args[0], path, req->data.args[2],
                            req->data.args[3]);

            if (fd >= 0) {

                struct seccomp_notif_addfd addfd;
                addfd.id = req->id;
                addfd.srcfd = fd;
                addfd.newfd = 42 + fd;
                addfd.flags = SECCOMP_ADDFD_FLAG_SETFD;
                addfd.newfd_flags = O_CLOEXEC;

                int remoteFd = ioctl(notifyFd, SECCOMP_IOCTL_NOTIF_ADDFD,
                                     &addfd);
                if (remoteFd == -1)
                    errExit("SECCOMP_IOCTL_NOTIF_ADDFD");

                close(fd);              /* No longer needed in supervisor */

                resp->error = 0;        /* "Success" */
                resp->val = remoteFd;

                printf("\tS: success! spoofed return = %lld\n", resp->val);

            } else {                    /* openat() failed in supervisor */

                resp->error = -errno;   /* Pass error back to target */
                printf("\tS: failure! (errno = %d; %s)\n", errno,
                        strerror(errno));
            }
        }

        /* Send a response to the notification */

        printf("\tS: sending response "
                "(flags = %#x; val = %lld; error = %d)\n",
                resp->flags, resp->val, resp->error);

        if (ioctl(notifyFd, SECCOMP_IOCTL_NOTIF_SEND, resp) == -1) {
            if (errno == ENOENT)
                printf("\tS: response failed with ENOENT; "
                        "perhaps target process's syscall was "
                        "interrupted by a signal?\n");
            else
                perror("ioctl-SECCOMP_IOCTL_NOTIF_SEND");
        }
    }
}

/* Implementation of the supervisor process:

   (1) obtains the seccomp notification file descriptor from 'sockPair[1]';
   (2) handles notifications that arrive on that file descriptor. */

static void
supervisor(int sockPair[2])
{
    int notifyFd = recvfd(sockPair[1]);
    if (notifyFd == -1)
        errExit("recvfd");

    closeSocketPair(sockPair);  /* We no longer need the socket pair */

    handleNotifications(notifyFd);
}

int
main(int argc, char *argv[])
{
    int sockPair[2];

    setbuf(stdout, NULL);

    /* Create a UNIX domain socket that is used to pass the seccomp
       notification file descriptor from the target process to the
       supervisor process. */

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, sockPair) == -1)
        errExit("socketpair");

    /* Create a child process--the "target"--that installs seccomp
       filtering. The target process writes the seccomp notification
       file descriptor onto 'sockPair[0]' and then calls openat(2) for
       each directory in the command-line arguments. */

    (void) targetProcess(sockPair, &argv[optind]);

    /* Catch SIGCHLD when target terminates, so that supervisor knows
       that there is nothing more to do. */

    struct sigaction sa;
    sa.sa_handler = sigchldHandler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
        errExit("sigaction");

    supervisor(sockPair);

    exit(EXIT_SUCCESS);
}
