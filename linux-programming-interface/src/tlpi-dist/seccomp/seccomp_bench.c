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

/* seccomp_bench.c

   A program to do some rough benchmarking for seccomp filtering.

   This program is run with the following command-line:

        seccomp_bench <num-loops> [<instr-cnt> [<instr> [num-filters]]]
                                       Defaults:  a           1

   The program loops calling getppid() 'num-loops' times after optionally
   installing seccomp filter(s).

   If just one command-line argument is supplied, then no BPF filter
   installed; this can be used to establish the baseline cost of the
   getppid() calls.

   If additional arguments are supplied, then a seccomp filter is installed
   before the getppid() loop is executed.  A filter is constructed that
   contains 'instr-cnt' instances of a specified instruction, plus a BPF_RET
   instruction to terminate the filter.

   The 'instr' argument specifies the instructions that are to be placed in
   the filter, and can be 'a' (BPF_ADD), 'l' (BPF_LD), or 'j' (BPF_JEQ).
   (However, see the comments below regarding load and jump instructions.)

   By default, one copy of the filter is installed into the kernel, but the
   optional 'num-filters' argument can be used to specify that multiple
   filter instances should be installed.

   To test with the in-kernel JIT compiler enabled:

        $ sudo sh -c "echo 1 > /proc/sys/net/core/bpf_jit_enable"

   (In more recent Linux distributions, the JIT compiler is enabled by
   default.)
*/
#define _GNU_SOURCE
#include <sys/syscall.h>
#include <linux/filter.h>
#include <linux/seccomp.h>
#include <sys/prctl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

static int
seccomp(unsigned int operation, unsigned int flags, void *arg)
{
    return syscall(__NR_seccomp, operation, flags, arg);
}

static struct sock_fprog*
create_filter(char instr, int icnt)
{
    struct sock_filter load = BPF_STMT(BPF_LD | BPF_W | BPF_ABS, 0);
    struct sock_filter jump = BPF_JUMP(BPF_JMP | BPF_JEQ | BPF_K, 0, 0, 0);
    struct sock_filter add = BPF_STMT(BPF_ALU | BPF_ADD | BPF_K, 1);
    struct sock_filter ret = BPF_STMT(BPF_RET | BPF_K, SECCOMP_RET_ALLOW);
    struct sock_filter instruction;

    /* Create a filter containing 'icnt' instructions of the kind specified
       in 'instr' */

    if (instr == 'a')
        instruction = add;
    else if (instr == 'j')
        instruction = jump;
    else if (instr == 'l')
        instruction = load;
    else {
        fprintf(stderr, "Bad instruction value: %c\n", instr);
        exit(EXIT_FAILURE);
    }

    /* NOTE: Nowadays, the kernel appears to be doing some optimization of the
       BPF filter. When the filter consists of "load" instructions (all of
       which, except the first, are effectively no-ops) or (the no-op) "jump"
       instructions, modifying the size of the filter seems to have no effect
       on the execution time. (Interestingly, if the load instruction is
       changed so that it loads from the operand (BPF_K), rather than the data
       area (BPF_ABS), then changing the size of the filter does have an
       effect on the execution time.)

       Presumably, the optimization is eliding away the no-op instructions
       This was not always the case: in older kernels, varying the size of
       such filters did change the execution time. I have not determined when
       the change occurred (or the location of the relevant code in the
       kernel), but some quick testing suggests that the change was no earlier
       than kernel 5.11 and no later than kernel 5.15. */

    int fsize = icnt + 1;
    struct sock_filter *filter = calloc(fsize, sizeof(struct sock_filter));
    if (filter == NULL)
        errExit("calloc");

    for (int j = 0; j < icnt; j++)
        filter[j] = instruction;

    /* Add a return instruction to terminate the filter */

    filter[icnt] = ret;

    /* Install the BPF filter */

    struct sock_fprog *prog = malloc(sizeof(struct sock_fprog));
    if (prog == NULL)
        errExit("malloc");

    prog->len = fsize;
    prog->filter = filter;

    printf("Total instructions in filter (including \"return\"): %d\n", fsize);

    return prog;
}

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <num-loops> [<instr-cnt> [<add|jump|load> "
                "[num-filters]]]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    if (argc > 2) {
        int icnt = atoi(argv[2]);
        char instr = (argc > 3) ? argv[3][0] : 'a';
        int nfilters = (argc > 4) ? atoi(argv[4]) : 1;

        if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0))
            errExit("prctl");

        struct sock_fprog *prog = create_filter(instr, icnt);

        for (int j = 0; j < nfilters; j++) {
            if (seccomp(SECCOMP_SET_MODE_FILTER, 0, prog) == -1)
                errExit("seccomp");
        }

        printf("Total number of filters added: %d\n", nfilters);
    }

    int nloops = atoi(argv[1]);

    for (int j = 0; j < nloops; j++)
        getppid();

    exit(EXIT_SUCCESS);
}
