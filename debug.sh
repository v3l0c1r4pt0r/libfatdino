#!/bin/bash
# debug.sh - file to debug demo-shared app

LD_LIBRARY_PATH=`pwd`/build:$LD_LIBRARY_PATH gdb build/demo-shared