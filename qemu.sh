#!/bin/sh
set -e
. ./iso.sh

qemu-system-$(./src/target-triplet-to-arch.sh $HOST) -cdrom makar.iso -serial stdio
./clean.sh
