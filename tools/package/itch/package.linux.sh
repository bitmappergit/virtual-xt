#!/bin/bash

PACKAGE_DEST=package/virtualxt
mkdir -p $PACKAGE_DEST/bin $PACKAGE_DEST/lib

cp virtualxt $PACKAGE_DEST/bin
cp -r doc/manual $PACKAGE_DEST
cp tools/floppies/freedos_itch.img $PACKAGE_DEST/freedos.img
cp tools/package/itch/virtualxt $PACKAGE_DEST
cp tools/package/itch/itch.linux.toml $PACKAGE_DEST/.itch.toml

# Copy libs
cp /usr/lib/x86_64-linux-gnu/libSDL2-2.0.so.0 $PACKAGE_DEST/lib
cp /usr/lib/x86_64-linux-gnu/libsndio.so.6.1 $PACKAGE_DEST/lib