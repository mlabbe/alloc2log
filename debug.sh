#!/bin/bash

PATH_SO=bin/linux/alloc2log.so

gdb bin/linux/alloctest \
    -x "set environment LD_PRELOAD $PATH_SO" \
    2>&1
