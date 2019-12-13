#!/bin/bash

PACKAGE_DEST=package/virtualxt
mkdir -p $PACKAGE_DEST/VirtualXT.app/Contents/MacOS $PACKAGE_DEST/VirtualXT.app/Contents/Resources $PACKAGE_DEST/VirtualXT.app/Contents/Frameworks

cp virtualxt $PACKAGE_DEST/VirtualXT.app/Contents/MacOS
cp tools/package/itch/Info.plist $PACKAGE_DEST/VirtualXT.app/Contents
cp doc/icon/icon.icns $PACKAGE_DEST/VirtualXT.app/Contents/Resources
cp -r doc/manual $PACKAGE_DEST/VirtualXT.app/Contents/MacOS
cp tools/package/itch/freedos.img $PACKAGE_DEST/VirtualXT.app
cp tools/package/itch/itch.osx.toml $PACKAGE_DEST/.itch.toml

# Copy frameworks
cp -r /Volumes/SDL2/SDL2.framework $PACKAGE_DEST/VirtualXT.app/Contents/Frameworks

# Patch lib path
install_name_tool -change @rpath/SDL2.framework/Versions/A/SDL2 @executable_path/../Frameworks/SDL2.framework/Versions/A/SDL2 $PACKAGE_DEST/VirtualXT.app/Contents/MacOS/virtualxt 