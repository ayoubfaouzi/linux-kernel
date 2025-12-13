/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2025.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU General Public License as published by the   *
* Free Software Foundation, either version 3 or (at your option) any      *
* later version. This program is distributed without any warranty.  See   *
* the file COPYING.gpl-v3 for details.                                    *
\*************************************************************************/

/* Supplementary program for Chapter 47 */

/* svsem_create.c

   Experiment with the use of semget() to create a System V semaphore set.

   Usage as shown in usageError().
*/
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include "tlpi_hdr.h"

static void
usageError(const char *progName, const char *msg)
{
    if (msg != NULL)
        fprintf(stderr, "%s", msg);
    fprintf(stderr, "Usage: %s [-cx] {-f pathname | -k key | -p} "
                            "num-sems [octal-perms]\n", progName);
    fprintf(stderr, "    -c           Use IPC_CREAT flag\n");
    fprintf(stderr, "    -x           Use IPC_EXCL flag\n");
    fprintf(stderr, "    -f pathname  Generate key using ftok()\n");
    fprintf(stderr, "    -k key       Use 'key' as key\n");
    fprintf(stderr, "    -p           Use IPC_PRIVATE key\n");
    exit(EXIT_FAILURE);
}

int
main(int argc, char *argv[])
{
    /* Parse command-line options and arguments */

    int numKeyFlags = 0;        /* Counts -f, -k, and -p options */
    int flags = 0;
    long lkey;
    key_t key;
    int opt;

    while ((opt = getopt(argc, argv, "cf:k:px")) != -1) {
        switch (opt) {
        case 'c':
            flags |= IPC_CREAT;
            break;

        case 'f':               /* -f pathname */
            key = ftok(optarg, 1);
            if (key == -1)
                errExit("ftok");
            numKeyFlags++;
            break;

        case 'k':               /* -k key (octal, decimal or hexadecimal) */
            if (sscanf(optarg, "%li", &lkey) != 1)
                cmdLineErr("-k option requires a numeric argument\n");
            key = lkey;
            numKeyFlags++;
            break;

        case 'p':
            key = IPC_PRIVATE;
            numKeyFlags++;
            break;

        case 'x':
            flags |= IPC_EXCL;
            break;

        default:
            usageError(argv[0], NULL);
        }
    }

    if (numKeyFlags != 1)
        usageError(argv[0], "Exactly one of the options -f, -k, "
                            "or -p must be supplied\n");

    if (optind >= argc)
        usageError(argv[0], "Must specify number of semaphores\n");

    int numSems = getInt(argv[optind], 0, "num-sems");

    unsigned int perms = (argc <= optind + 1) ? (S_IRUSR | S_IWUSR) :
                         getInt(argv[optind + 1], GN_BASE_8, "octal-perms");

    int semid = semget(key, numSems, flags | perms);
    if (semid == -1)
        errExit("semget");

    printf("%d\n", semid);      /* On success, display semaphore set ID */
    exit(EXIT_SUCCESS);
}
