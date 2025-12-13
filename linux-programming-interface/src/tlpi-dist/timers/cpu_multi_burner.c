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

/* cpu_multi_burner.c

   Usage: cpu_multi_burner period...

   This program creates one child process per command-line argument.
   Each child loops, consuming CPU, and, after each 'period' seconds
   of elapsed time, reports its process ID and rate of CPU consumption
   since the last report.

   For some experiments, it is useful to confine all processes to the
   same CPU, using taskset(1). For example:

        taskset 0x1 ./cpu_multi_burner 2 2

   See also cpu_multithread_burner.c.
*/
#include <time.h>
#include "tlpi_hdr.h"

static bool displayLoopCnt = false;

#define NANO 1000000000L

static long
timespecDiff(struct timespec a, struct timespec b)
{
    return (b.tv_sec - a.tv_sec) * NANO + b.tv_nsec - a.tv_nsec;
}

static void
burnCPU(float period)
{
    long step_size = NANO * period;
    long prev_step = 0;

    struct timespec base_real;
    if (clock_gettime(CLOCK_REALTIME, &base_real) == -1)
        errExit("clock_gettime");

    struct timespec prev_real = base_real;

    struct timespec prev_cpu;
    if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &prev_cpu) == -1)
        errExit("clock_gettime");

    long nloops = 0;
    while (1) {
        nloops++;
        struct timespec curr_real;
        if (clock_gettime(CLOCK_REALTIME, &curr_real) == -1)
            errExit("clock_gettime");

        long elapsed_real_nsec = timespecDiff(base_real, curr_real);
        long elapsed_real_steps = elapsed_real_nsec / step_size;

        if (elapsed_real_steps > prev_step) {
            struct timespec curr_cpu;
            if (clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &curr_cpu) == -1)
                errExit("clock_gettime");

            long diff_real_nsec = timespecDiff(prev_real, curr_real);
            long diff_cpu_nsec = timespecDiff(prev_cpu, curr_cpu);

            printf("%ld", (long) getpid());
            printf("  [t=%.2f (delta: %.2f)]",
                    (double) elapsed_real_nsec / NANO,
                    (double) diff_real_nsec / NANO);
            printf("  %%CPU = %5.2f",
                    (double) diff_cpu_nsec / diff_real_nsec * 100);
            /*
            printf("   totCPU = %ld.%03ld",
                    (long) curr_cpu.tv_sec, curr_cpu.tv_nsec / 1000000);
            */
            if (displayLoopCnt)
                printf("   (nloops = %ld)", nloops / 1000000);
            printf("\n");

            prev_cpu = curr_cpu;
            prev_real = curr_real;
            prev_step = elapsed_real_steps;
            nloops = 0;
        }
    }
}

static void
usageError(const char *progName)
{
    fprintf(stderr, "%s [-n] <period>...\n", progName);
    fprintf(stderr, "Creates one process per argument that reports "
            "CPU usage each <period> seconds.\n");
    fprintf(stderr, "<period> can be a floating-point number\n");
    fprintf(stderr, "\nThe '-n' option displays how many million loops were "
            "executed per <period>\n");
    exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
    int opt;
    while ((opt = getopt(argc, argv, "n")) != -1) {
        switch (opt) {
        case 'n':   displayLoopCnt = true;          break;
        default:    usageError(argv[0]);
        }
    }

    int nproc = argc - optind;

    if (nproc < 1)
        usageError(argv[0]);

    for (int j = 0; j < nproc; j++) {
        float period;

        switch (fork()) {
        case 0:
            sscanf(argv[j + optind], "%f", &period);
            burnCPU(period);
            exit(EXIT_SUCCESS);         /* NOTREACHED */

        case -1:
            errExit("fork");

        default:
            break;
        }
    }

    pause();

    exit(EXIT_SUCCESS);
}
