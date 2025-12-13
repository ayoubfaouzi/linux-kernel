#!/bin/sh
# A handy script to copy the consh scripts and the 'unshare' binary from
# the filesystem of an outer container to the 'lower' layer of the filesystem
# of an inner container.
#
# Example usage:
#
#	$ ../consh_setup.sh -c cg_1 -h pukaki lower ovly # Launch outer container
#       / # mkdir demo_inner
#       / # mount -t tmpfs tmpfs demo_inner
#       / # cd demo_inner
#       /demo_inner # create_lowerfs.sh lower
#       /demo_inner # copy_scripts_and_unshare.sh lower/bin
#       Copying  /bin/unshare /bin/create_lowerfs.sh /bin/consh_setup.sh /bin/consh_post_setup.sh /bin/consh_nw_setup.sh /bin/copy_scripts_and_unshare.sh
#       /demo_inner # consh_setup.sh -c cg_2 lower ovly

if [ $# -eq 0 ] ; then
    echo "Usage: $0 <dest> [file...]"
    exit 1
fi
dest=$1
shift 1

if [ $# -eq 0 ] ; then
    files=
    for f in unshare create_lowerfs.sh \
	    consh_setup.sh consh_post_setup.sh consh_nw_setup.sh \
	    copy_scripts_and_unshare.sh; do
        files="$files $(which $f)"
    done
else
    files=$*
fi
echo "Copying $files"

rm -f $dest/unshare
cp $files $dest
