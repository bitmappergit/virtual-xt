#!/bin/bash

if [ $PUSH_SNAP = '1' ]; then
    mkdir $HOME/.snapcraft
    echo $SNAPCRAFT_LOGIN_FILE | base64 --decode --ignore-garbage > $HOME/.snapcraft/snapcraft.cfg
    snapcraft login --with $HOME/.snapcraft/snapcraft.cfg
    snapcraft push *.snap --release beta
else
    cd "${TRAVIS_BUILD_DIR}/package"
    butler validate virtualxt
    butler login
    butler push virtualxt phix/virtualxt:${TRAVIS_OS_NAME} --userversion ${VXT_VERSION}.${TRAVIS_BUILD_ID}
    cd ..
fi