#!/bin/sh
set -e
. ./src/headers.sh

NPROC=$(nproc 2>/dev/null || echo 1)

for PROJECT in $PROJECTS; do
  (cd $PROJECT && DESTDIR="$SYSROOT" $MAKE -j"$NPROC" install)
done
