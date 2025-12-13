#!/bin/sh
#
# OverlayFS example

if [ $# -ne 1 ] ; then
    echo "Usage: $0 <directory-name>"
    exit 1
fi

set -e

mkdir $1
cd $1

mkdir upper lower1 lower2 work merged

echo 'From lower 1' > lower1/all_low
echo 'From lower 2' > lower2/all_low

echo 'From lower 1' > lower1/all
echo 'From lower 2' > lower2/all

echo 'From lower 2' > lower2/lower_2

echo 'From lower 1' > lower1/hidden

mkdir lower1/dir_1
touch lower1/dir_1/file

sudo mount -t overlay \
	 -o lowerdir=./lower1:./lower2,upperdir=./upper,workdir=./work \
	 overlay ./merged

echo 'Upper' >> merged/all
rm merged/hidden

# ls merged
