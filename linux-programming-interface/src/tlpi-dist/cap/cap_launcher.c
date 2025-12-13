/*************************************************************************\
*                  Copyright (C) Michael Kerrisk, 2025.                   *
*                                                                         *
* This program is free software. You may use, modify, and redistribute it *
* under the terms of the GNU General Public License as published by the   *
* Free Software Foundation, either version 3 or (at your option) any      *
* later version. This program is distributed without any warranty.  See   *
* the file COPYING.gpl-v3 for details.                                    *
\*************************************************************************/

/* Supplementary program for Chapter 39 */

/* cap_launcher.c

   Launch a program with the credentials (UIDs, GIDs, supplementary GIDs)
   of a specified user, and with the capabilities specified on the
   command line.

   In its default mode of operation, the program relies on the use of ambient
   capabilities, a feature that first appeared in Linux 4.3. In this case,
   the program to be launched should be an unprivileged program (i.e., not a
   set-UID program, a set-GID program, or program with capabilities attached).
   This is so, because ambient capabilities are ignored (discarded) when
   execing a privileged program.

   If the -A option is specified, then this program raises the specified
   capabilities only in the process inheritable set (and not in the ambient
   set) before launching the specified program. In this case, the program
   that is to be launched should have the corresponding file inheritable
   capabilities enabled.
*/
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#include <sys/prctl.h>
#include <sys/capability.h>
#include <linux/securebits.h>
#include <pwd.h>
#include <grp.h>
#include "cap_functions.h"      /* Defines modifyCapSetting() */
#include "tlpi_hdr.h"

static void
usage(char *pname)
{
    fprintf(stderr, "Usage: %s [-A] user cap,... cmd arg...\n", pname);
    fprintf(stderr, "\t'user' is the user with whose credentials the\n");
    fprintf(stderr, "\t\tprogram is to be launched\n");
    fprintf(stderr, "\t'cap,...' is the set of capabilities with which the\n");
    fprintf(stderr, "\t\tprogram is to be launched\n");
    fprintf(stderr, "\t'cmd' and 'arg...' specify the command plus "
                    "arguments\n");
    fprintf(stderr, "\t\tfor the program that is to be launched\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "\tOptions:\n");
    fprintf(stderr, "\t    -A  Raise the specified capabilities only in the "
                    "inheritable set\n");
    fprintf(stderr, "\t\t(and not in ambient set) before launching 'cmd'\n");
    exit(EXIT_FAILURE);
}

/* Set the supplementary group list, based on the groups recorded for 'user'
   in /etc/group. */

static void
setSupplementaryGroupList(char *user, gid_t gid)
{
    /* Find out how many supplementary groups the user is a member of */

    int ngroups = 0;
    getgrouplist(user, gid, NULL, &ngroups);

    /* Allocate an array for supplementary group IDs */

    gid_t *groups = calloc(ngroups, sizeof(gid_t));
    if (groups == NULL)
        errExit("calloc");

    /* Get supplementary group list of 'user' from the group database.
       In addition, 'gid' (the user's primary GID, which was obtained
       from /etc/passwd) is also added to the list if it is not otherwise
       present. */

    if (getgrouplist(user, gid, groups, &ngroups) == -1)
        errExit("getgrouplist");

    /* Set the supplementary group list */

    if (setgroups(ngroups, groups) == -1)
        errExit("setgroups");
}

/* Switch credentials (user ID, group ID, supplementary groups) to
   those for the user named in 'user' */

static void
setCredentials(char *user)
{
    /* Look up user in user database */

    struct passwd *pwd = getpwnam(user);
    if (pwd == NULL) {
        fprintf(stderr, "Unknown user: %s\n", user);
        exit(EXIT_FAILURE);
    }

    setSupplementaryGroupList(user, pwd->pw_gid);

    /* Set all group IDs to GID of this user */

    if (setresgid(pwd->pw_gid, pwd->pw_gid, pwd->pw_gid) == -1)
        errExit("setresgid");

    /* Set all user IDs to UID of this user */

    if (setresuid(pwd->pw_uid, pwd->pw_uid, pwd->pw_uid) == -1)
        errExit("setresuid");
}

static cap_value_t
capFromName(char *p)
{
    cap_value_t cap;
    if (cap_from_name(p, &cap) == -1) {
        fprintf(stderr, "Unrecognized capability name: %s\n", p);
        exit(EXIT_FAILURE);
    }

    return cap;
}

/* Raise a single capability in the process inheritable set and optionally
   also in the ambient set */

static void
raiseCap(cap_value_t cap, char *capName, bool raiseAmbient)
{
    /* Raise the capability in the inheritable set */

    if (modifyCapSetting(CAP_INHERITABLE, cap, CAP_SET) == -1) {
        fprintf(stderr, "Could not raise '%s' inheritable "
                "capability (%s)\n", capName, strerror(errno));
        exit(EXIT_FAILURE);
    }

    if (raiseAmbient) {

        /* Raise the capability in the ambient set */

        if (prctl(PR_CAP_AMBIENT, PR_CAP_AMBIENT_RAISE, cap, 0, 0) == -1) {
            fprintf(stderr, "Could not raise '%s' ambient "
                    "capability (%s)\n", capName, strerror(errno));
            exit(EXIT_FAILURE);
        }
    }
}

/* Add a set of capabilities to the process inheritable set
   and optionally also to the ambient set */

static void
raiseInheritableAndAmbientCaps(char *capList, bool raiseAmbient)
{
    /* Walk through the capabilities listed in the comma-delimited list
       of capability names in 'capList'. */

    for (char *capName = capList; (capName = strtok(capName, ","));
            capName = NULL) {
        cap_value_t cap = capFromName(capName);
        raiseCap(cap, capName, raiseAmbient);
    }
}

int
main(int argc, char *argv[])
{
    bool raiseAmbient = true;
    int opt;

    while ((opt = getopt(argc, argv, "A")) != -1) {
        switch (opt) {
        case 'A':
            raiseAmbient = false;
            break;
        default:
            fprintf(stderr, "Bad option\n");
            usage(argv[0]);
            break;
        }
    }
    if (argc < optind + 3)
        usage(argv[0]);

    if (geteuid() != 0)
        fatal("Must be run as root");

    /* Set the "no setuid fixup" securebit, so that when we switch to
       a nonzero UID, we don't lose capabilities */

    if (prctl(PR_SET_SECUREBITS, SECBIT_NO_SETUID_FIXUP, 0, 0, 0) == -1)
        errExit("prctl");

    setCredentials(argv[optind]);

    raiseInheritableAndAmbientCaps(argv[optind + 1], raiseAmbient);

    /* Execute the program (with arguments) named in the remainder of the
       command-line */

    execvp(argv[optind + 2], &argv[optind + 2]);
    errExit("execvp");
}
