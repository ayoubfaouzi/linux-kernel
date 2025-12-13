#!/bin/sh
#
# create_lowerfs.sh
#
# (C) 2025, Michael Kerrisk
#
# Licensed under the GNU General Public License version 2 or later

if [ $# -lt 1 ] ; then
    echo "Usage: $0 <dir>"
    exit 1
fi

# Create top-level directories that should appear under /

mkdir $1
cd $1
mkdir bin dev etc home proc root sys tmp usr var

# Populate /bin with busybox(1) + links

cd bin
cp $(which busybox) .
$PWD/busybox --install .
