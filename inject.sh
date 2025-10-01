#!/bin/bash

sudo gdb -batch-silent -p $(echo "$(pidof java)" | cut -d " " -f 1) -ex "call (void*)dlopen(\"$PWD/build/libgoober.so\", 2)"
