#!/bin/sh
#
# Move all realtime (RT) tasks in the cgroups version 2 unified hierarchy
# to the cgroup root directory. This can be useful because, before Linux 5.4,
# it was not possible to enable the "cpu" controller if there were RT tasks in
# nonroot cgroups.  This restriction wass in place because the cgroups v2
# "cpu" controller did not yet understand how to handle RT tasks.
#
# This script must be run as superuser.
#
# The script by default assumes that the cgroup v2 hierarchy
# is mounted at /sys/fs/cgroup/unified. If the hierarchy is
# mounted elsewhere, then that location should be specified as
# the (sole) command-line argument to the script.

# (C) 2019 Michael Kerrisk. Licensed under the GNU GPL v2 or later

if [ $# -gt 0 ] ; then
	cd $1
else
	cd /sys/fs/cgroup/unified
fi

# In the following, we discard standard error to eliminate error output
# that occurs when reading 'cgroup.procs' files in 'threaded' cgroups

nonroot_pids=$(cat $(find */ -name cgroup.procs) 2> /dev/null)

LOGFILE=/tmp/rt_tasks.$$.log
for p in $(ps -L -o "pid tid cls rtprio cmd" -p $nonroot_pids |
		awk '$1 == "PID" || $3 == "RR" || $3 == "FF" || $3 == "DLN"' |
			grep -v ’]$’ |
			tee $LOGFILE | awk '$1 != "PID" {print $1}'); do
	echo $p > cgroup.procs
done

if [ $(cat $LOGFILE | wc -l) -gt 1 ] ; then
	echo "Moved following RT processes to root cgroup:"
	echo
	cat $LOGFILE | sed 's/^/    /'
else
	echo "No RT processes found in nonroot cgroups"
fi
