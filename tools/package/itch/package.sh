#!/bin/bash

export PACKAGE_DEST=package/virtualxt
mkdir $PACKAGE_DEST

cp virtualxt $PACKAGE_DEST
cp doc/manual $PACKAGE_DEST
cp tools/package/itch/freedos.img $PACKAGE_DEST
cp tools/package/itch/unix.itch.toml $PACKAGE_DEST/.itch.toml