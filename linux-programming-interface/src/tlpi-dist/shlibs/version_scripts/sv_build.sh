#!/bin/sh
#
# sv_build.sh
#
set -v

# Build version 1 of shared library

gcc -g -c -fPIC -Wall sv_lib_v1.c
gcc -g -shared -o libsv.so sv_lib_v1.o -Wl,--version-script,sv_v1.map

# Build a binary that links against version 1 of the library

gcc -g -o p1 sv_prog.c libsv.so

# Build version 2 of shared library

gcc -g -c -fPIC -Wall sv_lib_v2.c
gcc -g -shared -o libsv.so sv_lib_v2.o -Wl,--version-script,sv_v2.map

# Build a binary that links against version 2 of the library

gcc -g -o p2 sv_prog.c libsv.so
