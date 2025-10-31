!#/bin/bash

set -x

DIR=$(pwd)/build

if [ ! -d  "$DIR"]; then
    mkdir -p "$DIR"
fi

rm -rf $(pwd)/build/*
cd $(pwd)/build &&
    cmake .. &&
    make