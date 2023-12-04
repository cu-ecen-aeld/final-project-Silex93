#!/bin/sh
module=mpl3115a2
device=mpl3115a2
cd `dirname $0`
# invoke rmmod with all arguments we got
rmmod $module || exit 1

# Remove stale nodes

rm -f /dev/${device}
