#!/bin/bash

HERE="$(dirname "$(readlink -f "${0}")")"
export LD_LIBRARY_PATH=${HERE}:$LD_LIBRARY_PATH
exec "${HERE}/virtualxt" "$@"