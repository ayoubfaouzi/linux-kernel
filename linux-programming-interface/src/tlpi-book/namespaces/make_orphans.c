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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int
main(int argc, char *argv[])
{
    int numOrphans = (argc > 1) ? atoi(argv[1]) : 1;
    int sleepTime =  (argc > 2) ? atoi(argv[2]) : 120;

    pid_t ppidOrig = getpid();

    for (int j = 0; j< numOrphans; j++) {
        switch (fork()) {
        case -1:
            perror("fork");
            exit(EXIT_FAILURE);

        case 0:         /* Child */
            while (getppid() == ppidOrig)       /* Am I an orphan yet? */
                usleep(100000);

            printf("Child  (PID=%ld) now an orphan (parent PID=%ld)\n",
                    (long) getpid(), (long) getppid());

            sleep(sleepTime);

            printf("Child  (PID=%ld) terminating\n", (long) getpid());
            exit(EXIT_SUCCESS);

        default:        /* Parent */
            break;
        }
    }

    /* Parent falls through to here. */

    printf("Parent (PID=%ld) terminating\n", (long) getpid());
    exit(EXIT_SUCCESS);
}
