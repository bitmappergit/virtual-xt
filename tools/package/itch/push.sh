#!/bin/bash

if [ $PUSH_SNAP = '1' ]; then
    echo "Push snap!"
else
    cd "${TRAVIS_BUILD_DIR}/package"
    butler validate virtualxt
    butler login
    butler push virtualxt phix/virtualxt:${TRAVIS_OS_NAME} --userversion ${VXT_VERSION}.${TRAVIS_BUILD_ID}
    cd ..
fi