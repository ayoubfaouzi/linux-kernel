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

#include <sys/wait.h>
#include "tlpi_hdr.h"

int
main(int argc, char *argv[])
{
    printf("Parent PID = %ld\n", (long) getpid());

    for (int j = 1; ; j++) {
        pid_t childPid = fork();

        if (childPid == 0)      /* Child falls out of loop */
            break;

        if (childPid == -1) {   /* Could not create another child? */
            errMsg("fork");
            break;              /* If so, parent falls out of loop */
        }

        printf("Child %d: PID = %ld\n", j, (long) childPid);
    }

    /* Parent and all children fall through to here */

    sleep(60);
    exit(EXIT_SUCCESS);
}
