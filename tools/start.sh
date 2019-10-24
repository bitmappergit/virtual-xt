#!/bin/sh

clear
stty cbreak raw -echo min 0
../virtualxt ../freedos.img
stty cooked echo
