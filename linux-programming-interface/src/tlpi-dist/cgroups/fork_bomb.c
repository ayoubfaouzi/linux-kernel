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

/* fork_bomb.c

   A fork-bomb program that can be useful when experimenting with
   the cgroups 'pids' controller.

   Usage: fork_bomb num-children [child-sleep-secs]

   This program uses fork(2) to create 'num-children' child processes that
   each sleep (i.e., stay in existence) for 'child-sleep-secs' (default:
   300) seconds.

   The program pauses until the user types "Enter" before creating any
   children.  The intention of this pause is to allow the user some time
   to manipulate the cgroup membership of the parent process before any
   child processes are created.
*/
#include <sys/wait.h>
#include "tlpi_hdr.h"

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        usageErr("%s num-children [child-sleep-secs]\n",
                argv[0]);
    }

    int numChildren = atoi(argv[1]);
    int childSleepTime = (argc > 2) ? atoi(argv[2]) : 300;

    printf("Parent PID = %ld\n", (long) getpid());

    printf("Parent pausing; hit ENTER when ready\n");

    getchar();

    printf("Creating %d children that will sleep %d seconds\n",
            numChildren, childSleepTime);

    int failed = 0;
    for (int j = 1; j <= numChildren && !failed; j++) {
        pid_t childPid = fork();
        switch (childPid) {
        case -1:
            errMsg("fork");
            failed = 1;
            break;
        case 0:
            sleep(childSleepTime);
            exit(EXIT_SUCCESS);
        default:
            printf("Child %d: PID = %ld\n", j, (long) childPid);
            break;
        }
    }

    printf("Waiting for all children to terminate\n");

    while (waitpid(-1, NULL, 0) > 0)
        continue;

    printf("All children terminated; bye!\n");

    exit(EXIT_SUCCESS);
}
