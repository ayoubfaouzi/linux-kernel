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

/* seccomp_functions.h

   Header file for seccomp_functions.c
*/
#include <linux/seccomp.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

int seccomp(unsigned int operation, unsigned int flags, void *args);

/* Check that the notification ID provided by a SECCOMP_IOCTL_NOTIF_RECV
   operation is still valid. It will no longer be valid if the target
   process has terminated or is no longer blocked in the system call that
   generated the notification (because it was interrupted by a signal).

   This operation can be used when doing such things as accessing
   /proc/PID files in the target process in order to avoid TOCTOU race
   conditions where the PID that is returned by SECCOMP_IOCTL_NOTIF_RECV
   terminates and is reused by another process. */

bool cookieIsValid(int notifyFd, uint64_t id);

/* Access the memory of the target process (req->pid) of a seccomp user-space
   notification in order to fetch the pathname referred to by the system call
   argument 'argNum' in 'req->data.args[]'. The pathname is returned in
   'path', a buffer of 'len' bytes allocated by the caller. (This buffer
   should be sized using PATH_MAX.)

   Returns 0 if the pathname is successfully fetched.
   On error, one of the negative values below is returned. */

#define GTP_BAD_READ        -1  /* Error opening/reading /proc/PID/mem */
#define GTP_ID_NOT_VALID    -2  /* Cookie check failed */
#define GTP_BAD_PATH        -3  /* Pathname read from target's memory
                                   is badly formed */

int getTargetPathname(struct seccomp_notif *req, int notifyFd,
        int argNum, char *path, size_t len);

/* Allocate buffers for the seccomp user-space notification request and
   response structures. It is the caller's responsibility to free the
   buffers returned via 'req' and 'resp'. */

void allocSeccompNotifBuffers(struct seccomp_notif **req,
        struct seccomp_notif_resp **resp, struct seccomp_notif_sizes *sizes);
