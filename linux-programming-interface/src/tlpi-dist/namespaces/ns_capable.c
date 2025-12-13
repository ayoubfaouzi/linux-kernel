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

/* ns_capable.c

   Test whether a process (identified by PID) might--subject to LSM (Linux
   Security Module) checks--have capabilities in a target namespace (identified
   either by a /proc/PID/ns/xxx file or by the PID of a process that is a
   member of a user namespace).

   Usage: ./ns_capable <source-pid> <namespace-file|target-pid>
*/
#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <limits.h>
#include <stdbool.h>
#include <ctype.h>
#include <sys/capability.h>

#ifndef NS_GET_USERNS
#define NSIO    0xb7
#define NS_GET_USERNS           _IO(NSIO, 0x1)
#define NS_GET_PARENT           _IO(NSIO, 0x2)
#define NS_GET_NSTYPE           _IO(NSIO, 0x3)
#define NS_GET_OWNER_UID        _IO(NSIO, 0x4)
#endif

#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                        } while (0)

#define fatal(msg)      do { fprintf(stderr, "%s\n", msg); \
                             exit(EXIT_FAILURE); } while (0)

/* Display capabilities of the process with the specified PID. */

static void
display_process_capabilities(pid_t pid)
{
    cap_t caps = cap_get_pid(pid);
    if (caps == NULL)
        errExit("cap_get_proc");

    char *cap_string = cap_to_text(caps, NULL);
    if (cap_string == NULL)
        errExit("cap_to_text");

    printf("    Capabilities: %s\n", cap_string);

    cap_free(caps);
    cap_free(cap_string);
}

/* Obtain the effective UID of the process 'pid' by scanning its
   /proc/PID/status file. */

static uid_t
euid_of_process(pid_t pid)
{
    char path[PATH_MAX];

    snprintf(path, sizeof(path), "/proc/%ld/status", (long) pid);

    FILE *fp = fopen(path, "r");
    if (fp == NULL)
        errExit("fopen-/proc/PID/status");

    for (;;) {
        char line[1024];

        if (fgets(line, sizeof(line), fp) == NULL) {

            /* We reached EOF without finding "Uid:" record (should never
               happen). */

            fprintf(stderr, "Failure scanning for 'Uid:' in %s\n", path);
            exit(EXIT_FAILURE);
        }

        int uid;
        if (strstr(line, "Uid:") == line) {
            sscanf(line, "Uid: %*d %d %*d %*d", &uid);
            fclose(fp);
            return uid;
        }
    }
}

/* Return true if two file descriptors refer to the same namespace,
   otherwise false. */

static bool
ns_equal(int nsfd1, int nsfd2)
{
    struct stat sb1, sb2;

    if (fstat(nsfd1, &sb1) == -1)
        errExit("fstat-nsfd1");
    if (fstat(nsfd2, &sb2) == -1)
        errExit("fstat-nsfd2");

    /* Namespaces are equal if *both* the device ID and the inode number
       in the 'stat' records match. */

    return sb1.st_dev == sb2.st_dev && sb1.st_ino == sb2.st_ino;
}

/* Return a CLONE_NEW* value, indicating the type of namespace referred to by
   the file descriptor 'ns_fd'. */

static int
ns_type(int ns_fd)
{
    int nstype = ioctl(ns_fd, NS_GET_NSTYPE);
    if (nstype == -1)
        errExit("ioctl-NS_GET_NSTYPE");

    return nstype;
}

/* Return a file descriptor for the user namespace that owns the namespace
   referred to by the file descriptor 'ns_fd'. */

static int
owning_userns_of(int ns_fd)
{
    int userns_fd = ioctl(ns_fd, NS_GET_USERNS);
    if (userns_fd == -1)
        errExit("ioctl-NS_GET_USERNS");

    return userns_fd;
}

/* Return the UID of the creator of the namespace referred to by the
   file descriptor 'userns_fd'. */

static int
uid_of_userns(int userns_fd)
{
    uid_t owner_uid;

    if (ioctl(userns_fd, NS_GET_OWNER_UID, &owner_uid) == -1) {
        perror("ioctl-NS_GET_OWNER_UID");
        exit(EXIT_FAILURE);
    }

    return owner_uid;
}

/* Determine whether 'source_userns' refers to an ancestor user namespace of
   the user namespace referred to by 'target_userns'.

   Returns:
   * -1 if 'source_userns' does not refer to an ancestor user namespace;
   * otherwise, if 'source_userns' does refer to an ancestor user namespace,
     then a file descriptor (a value >= 0) that refers to the user namespace
     that is the immediate descendant of 'source_userns' in the chain of user
     namespaces from 'source_userns' to 'target_userns'. (Note that the
     returned file descriptor may be the same as 'target_userns' if
     'target_userns' is an immediate child of 'source_userns'). */

static int
is_ancestor_userns(int source_userns, int target_userns)
{
    /* Starting at the parent of the namespace referred to by 'target_userns',
       we walk upward through the chain of ancestor namespaces until we can
       traverse no further, or until we find a namespace that is the same as
       the one referred to by 'source_userns'. */

    int child = target_userns;

    for (;;) {
        int parent = ioctl(child, NS_GET_PARENT);

        /* If the attempt to fetch the parent of the user namespace 'child'
           fails with the error EPERM, then this is the initial namespace. In
           other words, we traversed as far as we could, and did not find
           'source_userns' in the chain of ancestors of 'target_userns'. */

        if (parent == -1) {

            /* Failing with any error other than EPERM is unexpected, so we
               terminate. */

            if (errno != EPERM)
                errExit("ioctl-NS_GET_PARENT");

            if (child != target_userns) {
                if (close(child) == -1)
                    errExit("close(child)");
            }

            return -1;
        }

        /* If 'parent' and 'source_userns' are the same namespace, then we need
           traverse no further in the series of user namespace ancestors:
           'source_userns' does refer to an ancestor of 'target_userns'. */

        if (ns_equal(parent, source_userns)) {
            if (close(parent) == -1)
                errExit("close(parent)");

            return child;
        }

        /* Otherwise, check the next ancestor user namespace. */

        if (child != target_userns) {
            if (close(child) == -1)
                errExit("close(child) [loop]");
        }

        child = parent;
    }
}

/* Return a file descriptor that refers to the user namespace of the process
   with the ID 'pid'. */

static int
userns_from_pid(pid_t pid)
{
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "/proc/%ld/ns/user", (long) pid);

    int userns = open(path, O_RDONLY);
    if (userns == -1)
        errExit("open-pid-userns");

    return userns;
}

/* Return a file descriptor for the user namespace corresponding to 'arg'.
   'arg' is either a PID or the pathname of a /proc/PID/ns/xxx symlink
   (or a bind mount to such a symlink). */

static int
get_userns_from(char *arg)
{
    /* If 'arg' starts with a digit, we assume it is a PID and return the
       user namespace of that PID. */

    if (isdigit(arg[0]))
        return userns_from_pid(atoi(arg));

    /* Obtain a file descriptor that refers to the namespace specified by
       'arg'. */

    int ns = open(arg, O_RDONLY);
    if (ns == -1)
        errExit("open-ns-file");

    /* In order to determine whether the process has capabilities in a
       namespace, we must determine the relevant user namespace,
       which is 'ns' itself if 'ns' refers to a user namespace,
       otherwise the user namespace that owns 'ns'. */

    int userns;
    if (ns_type(ns) == CLONE_NEWUSER) {
        userns = ns;
    } else {
        userns = owning_userns_of(ns);
        if (close(ns) == -1)            /* No longer need this FD */
            errExit("close-ns");
    }

    return userns;
}

int
main(int argc, char *argv[])
{
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <source-PID> <ns-file|target-PID>\n",
                argv[0]);
        fprintf(stderr, "\t'ns-file' is a /proc/PID/ns/xxxx file\n");
        exit(EXIT_FAILURE);
    }

    pid_t pid = atoi(argv[1]);

    int source_userns = userns_from_pid(pid);
    int target_userns = get_userns_from(argv[2]);

    if (ns_equal(source_userns, target_userns)) {
        /* The source process is in the target user namespace. */

        printf("PID %ld is in the target namespace. Subject to LSM checks, "
                "it has the\n"
                "capabilities that are in its sets, which are:\n\n",
                (long) pid);

        display_process_capabilities(pid);
    } else {

        /* Check to see if the source process is in an ancestor user
           namespace. */

        int child_userns = is_ancestor_userns(source_userns, target_userns);

        if (child_userns == -1) {

            /* The source process is not in an ancestor user namespace of
               'target_userns'. */

            printf("PID %ld is not in an ancestor user namespace.\n"
                    "Therefore, it has no capabilities in the target "
                    "namespace.\n", (long) pid);
        } else {

            /* The source process is in a user namespace that is an ancestor of
               the target user namespace, and 'child_userns' refers to the
               immediate descendant of the source process's user namespace in
               the chain of user namespaces from the user namespace of the
               source process to the target user namespace. If the effective
               UID of the source process matches the owner UID of
               'child_userns', then the source process has all capabilities in
               the descendant namespace(s); otherwise, it just has the
               capabilities that are in its sets. */

            bool is_owner = euid_of_process(pid) == uid_of_userns(child_userns);

            printf("PID %ld is in an ancestor user namespace", (long) pid);

            if (is_owner) {
                printf(" and its effective UID matches\n");
            } else {
                printf(", but its effective UID does not match\n");
            }

            printf("the owner of the immediate child user "
                    "namespace of that ancestor namespace.\n");

            if (is_owner) {
                printf("Therefore, subject to LSM checks, it has all "
                        "capabilities in the target\n"
                        "namespace!\n");
            } else {
                printf("Therefore, subject to LSM checks, it has only the "
                        "capabilities that are in its\n"
                        "sets, which are:\n\n");
                display_process_capabilities(pid);
            }

            if (child_userns != target_userns) {    /* Prevent double close() */
                if (close(child_userns) == -1)
                    errExit("close-child_userns");
            }
        }
    }

    if (close(target_userns) == -1)
        errExit("close-target_userns");
    if (close(source_userns) == -1)
        errExit("close-source_userns");

    exit(EXIT_SUCCESS);
}
