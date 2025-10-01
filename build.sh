#!/bin/bash

reset_thing=0

if ! [ -e "build" ]; then
    reset_thing=1
    mkdir build
fi

cd build/
cmake ..
make all
cd ..

if [ $reset_thing -eq 1 ]; then
    cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
fi
