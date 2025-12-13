#!/bin/sh
#
# consh_post_setup.sh
#
# (C) 2025, Michael Kerrisk
#
# Licensed under the GNU General Public License version 2 or later

# In the following steps, we create the mount that will be used as the root
# filesystem in the container. The mount is a union mount built by using
# OverlayFS to combine a set of directories created under <fs-dir>.
# (Since Linux 5.11 (2021), it possible to create such a mount in noninitial
# user + mount NSs created by unprivileged users.)

if [ -n "$VERBOSE" ] ; then
    echo "=========="
fi

unset ENV

mkdir -p $OVLY_DIR

if [ -n "$VERBOSE" ] ; then
    echo "Creating overlay dirs in '$OVLY_DIR'"
fi
mkdir $OVLY_DIR/upper $OVLY_DIR/work
	# Avoid "mkdir -p $OVLY_DIR/{upper,work}"
	# because not supported by busybox shell

mkdir $OVLY_DIR/merged		# Create mount point

if [ -n "$VERBOSE" ] ; then
    echo "Creating overlay mount at '$OVLY_DIR/merged'"
fi
mount -t overlay \
		-o lowerdir=$LOWER \
		-o upperdir=$OVLY_DIR/upper \
		-o  workdir=$OVLY_DIR/work \
		    overlay $OVLY_DIR/merged

cd $OVLY_DIR
unset LOWER OVLY_DIR		# No longer needed

# As a demonstration, create a file that is private to this container.

manifest=merged/MANIFEST
echo "Created at:  $(date)"   > $manifest
echo "Creator UID: $(id -u)" >> $manifest
echo "Creator PID: $$"       >> $manifest

# Set up the root filesystem for the container.

if [ -n "$VERBOSE" ] ; then
    echo "Preparing for pivot root"
fi

mount --make-rprivate /
mount --bind merged merged
mkdir merged/oldrootfs

if [ -n "$VERBOSE" ] ; then
    echo "Pivoting root (pivoting to 'merged'; placing old root FS at merged/oldrootfs/)"
fi

pivot_root merged merged/oldrootfs/

# Our current directory is outside the root directory. Switch it to new
# root directory.

cd /

# Mount various namespace-related pseudofilesystems.

if [ -n "$VERBOSE" ] ; then
    echo "Mounting filesystems"
fi

mount -t proc proc /proc

mount -t sysfs sysfs /sys
mount -t cgroup2 cgroup2 /sys/fs/cgroup

mkdir -p /dev/mqueue
mount -t mqueue mqueue /dev/mqueue

# In Podman and Docker, there is a 'tmpfs' mount at /dev. However, I found
# that if I simply do the following before creating the devices (and the
# /dev/mqueue mount):
#
#	mount -t tmpfs tmpfs /dev
#
# then the devices don't have the right behavior. In particular, various
# devices get errors such as the following:
#
#       $ date > /dev/null
#	sh: can't create /dev/null: Permission denied
#
# I have not dug deep enough to work out why this happens. Perhaps it
# relates to the notion of "locked mounts" described in mount_namespaces(7).
# One interesting thing I noticed was that if I instead started the
# container without creating any /dev mounts in this script, then from
# another terminal window (in the initial user namespace) I could use
# "nsenter -t PID -m -p" to step into the mount namespace of the container
# and then successfully mount tmpfs at /dev and then bind mount the devices.
# Podman does a dance in order to mount tmpfs at /dev while at the same time
# avoiding this problem; I've yet to dig into the details.

if [ -n "$VERBOSE" ] ; then
    echo "Creating devices"
fi

for name in full null random tty urandom zero; do
    touch /dev/$name
    mount --bind oldrootfs/dev/$name /dev/$name
done

# Instead of making bind mounts as above, after the container has been started
# it would also be possible to make the devices by performing mknod(1) in the
# appropriate directory, from a privileged shell in the initial user namespace.
# (As noted in user_namespaces(7) mknod(1) can be used only by a process that
# is privileged in the initial user namespace.)

# We could also do the following:
#
#    mkdir /dev/pts
#    mount -t devpts devpts /dev/pts     # Pseudoterminal FS
#    ln -s /dev/pts/ptmx /dev/ptmx
#
#    mkdir /dev/shm
#    mount -t tmpfs shm /dev/shm         # For POSIX shared memory

# The umount of oldrootfs needs to be done after creating the /proc mount.
# As far as I can tell, the reason is that outlined in this mail message:
# https://lists.linuxfoundation.org/pipermail/containers/2018-April/038840.html
# https://lore.kernel.org/lkml/87tvsrjai0.fsf@xmission.com/T/
# That message notes:
#
#     Since Linux v4.2 with commit 1b852bceb0d1 ("mnt: Refactor the logic for
#     mounting sysfs and proc in a user namespace"), new mounts of proc or
#     sysfs in non init userns are only allowed when there is at least one
#     fully-visible proc or sysfs mount.
#
# The kernel commit is:
#
#   commit 1b852bceb0d111e510d1a15826ecc4a19358d512
#   Author: Eric W. Biederman <ebiederm@xmission.com>
#   Date:   Fri May 8 23:22:29 2015 -0500
#
#       mnt: Refactor the logic for mounting sysfs and proc in a user namespace

# At this point, we no longer need the old root FS, so unmount it.

if [ -n "$VERBOSE" ] ; then
    echo "Unmounting 'oldrootfs'"
fi

umount -l oldrootfs
rmdir oldrootfs

# Set the hostname

hostname $HOSTNAME

# Create README file inside the container with some suggestions about
# commands to try out inside the container.

cat << 'EOF' > README
# Try some of the following commands, to see how things look:

mount
ps
ls /proc
echo $$
readlink /proc/$$/ns/user
cat /proc/$$/uid_map
id
grep Cap /proc/$$/status
hostname
hostname new-host-name
cat /proc/$$/cgroup
ls /bin
EOF

unset VERBOSE
