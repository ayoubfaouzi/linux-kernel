/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2025.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU General Public License as published by the   *
* Free Software Foundation, either version 3 or (at your option) any      *
* later version. This program is distributed without any warranty.  See   *
* the file COPYING.gpl-v3 for details.                                    *
\*************************************************************************/

/* Supplementary program for Chapter 23 */

/* cpu_burner.c

   A small program that simply consumes CPU time, displaying the rate of CPU
   consumption during each second.
*/
#include <time.h>
#include "tlpi_hdr.h"

#define NANO 1000000000L

static long
timespecDiff(struct timespec a, struct timespec b)
{
    return (b.tv_sec - a.tv_sec) * NANO + b.tv_nsec - a.tv_nsec;
}

int
main(int argc, char *argv[])
{
    struct timespec curr_real, prev_cpu;

    struct timespec prev_real;
    if (clock_gettime(CLOCK_REALTIME, &prev_real) == -1)
        errExit("clock_gettime");
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &prev_cpu) == -1)
        errExit("clock_gettime");

    /* Loop consuming CPU time. */

    int cnt = 0;
    while (1) {
        if (clock_gettime(CLOCK_REALTIME, &curr_real) == -1)
            errExit("clock_gettime");

        long elapsed_real_nsec = timespecDiff(prev_real, curr_real);

        /* Each time the real time clock ticks over to another second,
           display the rate of CPU consumption in the interval since the
           previous second. */

        if (elapsed_real_nsec >= NANO) {
            struct timespec curr_cpu;
            if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &curr_cpu) == -1)
                errExit("clock_gettime");

            long elapsed_cpu_nsec = timespecDiff(prev_cpu, curr_cpu);

            printf("[%ld]  %%CPU = %5.2f (%d)\n", (long) getpid(),
                    (double) elapsed_cpu_nsec / elapsed_real_nsec * 100.0,
                    cnt);

            prev_real = curr_real;
            prev_cpu = curr_cpu;
            cnt++;
        }
    }

    exit(EXIT_SUCCESS);
}
