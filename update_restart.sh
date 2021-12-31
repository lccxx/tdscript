#!/bin/bash

set -ex

git pull
git submodule update --init

export CC=clang
export CXX=clang

cmake -DCMAKE_BUILD_TYPE=Release -DWITH_TESTS=OFF -DWITH_STATIC=ON -DWITH_IPV6=ON -S . -B build-release

cmake --build build-release

systemctl --user stop tdscript

cp build-release/tdscript ../bin/

systemctl --user start tdscript
