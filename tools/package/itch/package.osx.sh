#!/bin/bash

PACKAGE_DEST=package/virtualxt
mkdir -p $PACKAGE_DEST $PACKAGE_DEST/bin $PACKAGE_DEST/lib

cp virtualxt $PACKAGE_DEST/bin
cp -r doc/manual $PACKAGE_DEST
cp tools/package/itch/freedos.img $PACKAGE_DEST
cp tools/package/itch/virtualxt $PACKAGE_DEST
cp tools/package/itch/unix.itch.toml $PACKAGE_DEST/.itch.toml

# Copy frameworks
cp -r /Volumes/SDL2/SDL2.framework $PACKAGE_DEST/bin