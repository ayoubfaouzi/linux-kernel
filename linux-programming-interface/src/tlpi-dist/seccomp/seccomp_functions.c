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

/* seccomp_functions.c

   Some useful functions for programs that use seccomp().
*/
#define _GNU_SOURCE
#include <fcntl.h>
#include <limits.h>
#include <linux/audit.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>
#include "seccomp_functions.h"
#include "tlpi_hdr.h"

int
seccomp(unsigned int operation, unsigned int flags, void *args)
{
    return syscall(__NR_seccomp, operation, flags, args);
}

/* Check that the notification ID provided by a SECCOMP_IOCTL_NOTIF_RECV
   operation is still valid. It will no longer be valid if the target
   process has terminated or is no longer blocked in the system call that
   generated the notification (because it was interrupted by a signal).

   This operation can be used when doing such things as accessing
   /proc/PID files in the target process in order to avoid TOCTOU race
   conditions where the PID that is returned by SECCOMP_IOCTL_NOTIF_RECV
   terminates and is reused by another process. */

bool
cookieIsValid(int notifyFd, uint64_t id)
{
    return ioctl(notifyFd, SECCOMP_IOCTL_NOTIF_ID_VALID, &id) == 0;
}

/* Access the memory of the target process (req->pid) of a seccomp user-space
   notification in order to fetch the pathname referred to by the system call
   argument 'argNum' in 'req->data.args[]'. The pathname is returned in
   'path', a buffer of 'len' bytes allocated by the caller. (This buffer
   should be sized using PATH_MAX.)

   Returns 0 if the pathname is successfully fetched.
   On error, one of the negative values below is returned. */

int
getTargetPathname(struct seccomp_notif *req, int notifyFd,
        int argNum, char *path, size_t len)
{
    char procMemPath[PATH_MAX];

    snprintf(procMemPath, sizeof(procMemPath), "/proc/%d/mem", req->pid);

    int procMemFd = open(procMemPath, O_RDONLY | O_CLOEXEC);
    if (procMemFd == -1)
        return GTP_BAD_READ;

    /* Check that the process whose info we are accessing is still alive and
       blocked in the system call that caused the notification. If this check
       succeeds, we know that the /proc/PID/mem file descriptor that we opened
       corresponded to the process for which we received a notification. */

    if (!cookieIsValid(notifyFd, req->id)) {
        close(procMemFd);
        return GTP_ID_NOT_VALID;
    }

    /* Read bytes at the location containing the pathname argument. */

    ssize_t nread = pread(procMemFd, path, len, req->data.args[argNum]);

    close(procMemFd);

    if (nread <= 0)
        return GTP_BAD_READ;

    /* Once again check that the notification ID is still valid. The case we
       are particularly concerned about here is that just before we fetched
       the pathname, the target's blocked system call was interrupted by a
       signal handler, and after the handler returned, the target carried on
       execution (past the interrupted system call). In that case, we have no
       guarantees about what we are reading, since the target's memory may
       have been arbitrarily changed by subsequent operations. */

    if (!cookieIsValid(notifyFd, req->id))
        return GTP_ID_NOT_VALID;

    /* Even if the target's system call was not interrupted by a signal, we
       have no guarantees about what was in the memory of the target process.
       (The memory may have been modified by another thread, or even by an
       external attacking process.) We therefore treat the buffer returned by
       pread() as untrusted input. The buffer should be terminated by a null
       byte; if not, then we will trigger an error for the target process. */

    if (strnlen(path, nread) < nread)
        return 0;       /* Success */
    else
        return GTP_BAD_PATH;
}

/* Allocate buffers for the seccomp user-space notification request and
   response structures. It is the caller's responsibility to free the
   buffers returned via 'req' and 'resp'. */

void
allocSeccompNotifBuffers(struct seccomp_notif **req,
        struct seccomp_notif_resp **resp,
        struct seccomp_notif_sizes *sizes)
{
    /* Discover the sizes of the structures that are used to receive
       notifications and send notification responses, and allocate
       buffers of those sizes. */

    if (seccomp(SECCOMP_GET_NOTIF_SIZES, 0, sizes) == -1)
        errExit("seccomp-SECCOMP_GET_NOTIF_SIZES");

    *req = malloc(sizes->seccomp_notif);
    if (*req == NULL)
        errExit("malloc-seccomp_notif");

    /* When allocating the response buffer, we must allow for the fact
       that the user-space binary may have been built with user-space
       headers where 'struct seccomp_notif_resp' is bigger than the
       response buffer expected by the (older) kernel. Therefore, we
       allocate a buffer that is the maximum of the two sizes. This
       ensures that if the supervisor places bytes into the response
       structure that are past the response size that the kernel expects,
       then the supervisor is not touching an invalid memory location. */

    size_t resp_size = sizes->seccomp_notif_resp;
    if (sizeof(struct seccomp_notif_resp) > resp_size)
        resp_size = sizeof(struct seccomp_notif_resp);

    *resp = malloc(resp_size);
    if (resp == NULL)
        errExit("malloc-seccomp_notif_resp");

}
