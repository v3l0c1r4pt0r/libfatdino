#!/bin/bash
# run.sh - script to run demo-shared app

LD_LIBRARY_PATH=`pwd`/build:$LD_LIBRARY_PATH build/demo-shared "$@"