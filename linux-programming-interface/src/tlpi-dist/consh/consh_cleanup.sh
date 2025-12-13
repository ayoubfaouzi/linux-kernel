#!/bin/sh
#
# consh_cleanup.sh
#
# (C) 2025, Michael Kerrisk
#
# Licensed under the GNU General Public License version 2 or later

usage()
{
cat << EOF 1>&2

Usage: $0 [-c <cgroup>] <overlay-dir>

<overlay-dir> is the location containing the components of the OverlayFS mount
	(lower, upper, work, merged). This directory subtree will be removed.
-c <cgroup> is the cgroup directory used for the container. This directory will
	be removed.
EOF
    exit 1
}

cgroup=""
while getopts ":c:" optname "$@"; do
    case "$optname" in
    c)  cgroup=$OPTARG
        ;;
    *)  echo "Unknown option: $OPTARG" 1>&2
	usage
        ;;
    esac
done

shift $(expr $OPTIND - 1)

if [ $# -lt 1 ] ; then
    echo "Missing argument" 1>&2
    usage
fi

rm -rf $ovly_dir

if [ "X$cgroup" != "X" ] ; then
    cg_mount="/sys/fs/cgroup"
    uslice="$cg_mount/user.slice/user-$(id -u).slice/user@$(id -u).service"
    if [ -d $uslice/$cgroup ] ; then
        rmdir $uslice/cgroup
    elif [ -O $cg_mount/$cgroup ] ; then
        rmdir $cg_mount/$cgroup
    else
        echo 1>&2 "Could not find cgroup $cgroup"
        exit 1
    fi
fi
