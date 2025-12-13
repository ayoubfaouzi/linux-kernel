#!/bin/sh
#
# consh_setup.sh
#
# (C) 2025, Michael Kerrisk
#
# Licensed under the GNU General Public License version 2 or later

usage()
{
cat << EOF 1>&2

Usage: $0 [options] <lower-dir> <overlay-dir>

<lower-dir> is the pathname of an existing directory that will be used as the
        lower layer in the OverlayFS union mount for the container.
<overlay-dir> is the pathname that will be used create a new directory where
	the other pieces (upper, work, merged) of the OverlayFS union mount
	for the container will be set up. The overlay mount will be created
	at '<overlay-dir>/merged'

Options are as follows:
    -c <cgroup>
	Can be used specify the pathname of a cgroup into which the container
	should be placed. The pathname is interpreted relative to the cgroup
	root directory, and the cgroup is created if it does not already exist.
    -p <cgroup-parent-path>
	Can be used to specify the cgroup parent under which <cgroup> is
	created.
    -h <hostname>
	Can be used to specify the hostname that will be used in the container.
EOF
    exit 1
}

# Command-line option parsing

cgroup=""
host="consh-host"
export VERBOSE=""
while getopts ":c:h:p:v" optname "$@"; do
    case "$optname" in
    c)  cgroup=$OPTARG
        ;;
    h)  host=$OPTARG
        ;;
    p)  cgroup_parent=$OPTARG
        ;;
    v)  VERBOSE="x"
        ;;
    *)  echo "Unknown option: $OPTARG" 1>&2
	usage
        ;;
    esac
done

shift $(expr $OPTIND - 1)

if [ $# -lt 2 ] ; then
    echo "Missing arguments" 1>&2
    usage
fi

lower=$1
ovly_dir=$2

if ! [ -e $lower ] ; then
    echo "Error: '$lower' does not exist!" 1>&2
    exit 1
fi

# A check so that during demos I don't forget to make a tmpfs mount
# when doing container-in-a-container demos. (OverlayFS doesn't allow
# the 'upper' layer in a mount to itself be an OverlayFS mount.)

if df -T $lower | grep -q overlay || \
       df -T $(dirname $ovly_dir) | grep -q overlay; then
    cat 1>&2 << EOF
It looks like you might be trying to run a container inside a container.
You should start over again, first creating a tmpfs mount for the inner
container's overlay filesystem. Bye!
EOF
    exit 1
fi

# If a cgroup was specified, create that cgroup and move this shell into the
# cgroup.

# If 'cgroup_parent' was explicitly specified, then we will create the cgroup
# under that parent cgroup. If the parent was not specified, we use a default.
# On a systemd-based system, we should be able to create the cgroup under
# /sys/fs/cgroup/user.slice/user-$(id -u).slice/user@$(id -u).service/, which
# is a cgroup that has been delegated to this user. (Here, "delegated" means
# that the ownership of the cgroup has been changed to this user's ID, with
# the result that this user can manage this subtree of the cgroup hierarchy.)
# If that cgroup path is not present, then we'll try /sys/fs/cgroup.
#

if [ "X$cgroup" != "X" ] ; then
    if [ "X$cgroup_parent" = "X" ] ; then
        cg_mount="/sys/fs/cgroup"
	uslice="$cg_mount/user.slice/user-$(id -u).slice/user@$(id -u).service"
        if [ -O $uslice ] ; then
	    cgroup_parent=$uslice
	elif [ -O $cg_mount ] ; then
	    cgroup_parent=$cg_mount
	else
	    echo 1>&2 "$cgroup_parent is not owned by $USER"
	    exit 1
	fi
    fi

    cgpath="$cgroup_parent/$cgroup"
    if [ -n "$VERBOSE" ] ; then
        echo "Creating cgroup: $cgpath"
    fi

    mkdir -p $cgpath

    sh -c "echo $$ > $cgpath/cgroup.procs"
fi

# We use unshare(1) to launch a shell in new namespaces. Some points deserve
# explanation:
#
# - We use busybox to launch the shell. This ensures that (a) the shell is
#   statically linked (and thus has no external shared library dependencies),
#   and (b) we are running the same shell as would be run by using 'sh' inside
#   the "container".
#
# - We set the ENV environment to point to a script that will be executed as
#   a start-up script by the new shell. This script performs the set-up steps
#   that are required after the namespaces have been created.
#
# - We use the --fork option to ensure that a child process is created.
#   That child will have PID in the new PID namespace.
#
# - One blemish in this set-up is that the "unshare" parent process will
#   also be in the cgroup. I can see no tidy way to avoid this (at least
#   not if we want to run an interactive shell in the container and we use the
#   standard unshare(1) command to do the set-up). Consequently, the only way
#   to remove the parent process from the cgroup would be via a manual step
#   in a separate terminal window.
#
# - We use the shell 'exec' built-in, so that the shell executing this script
#   is replaced by the "unshare" executable. This allows us to avoid the
#   creation of yet another process that would be a member of the cgroup.
#
if [ -n "$VERBOSE" ] ; then
    echo "Using 'unshare' to launch a shell in new namespaces"
fi

exec env -i HOME="/root" PATH="/usr/sbin:/usr/bin:/sbin:/bin" \
	    HOSTNAME="$host" \
	    LOWER="$lower" \
	    OVLY_DIR="$ovly_dir" \
	    VERBOSE="$VERBOSE" \
	    ENV=$(dirname $0)/consh_post_setup.sh \
	unshare --user --map-root-user --pid --fork \
	        --mount --net --ipc --uts --cgroup \
            busybox sh
