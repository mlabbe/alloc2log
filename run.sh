#!/bin/bash

PATH_ROOT="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null && pwd )"
PATH_SO="$PATH_ROOT/bin/linux/alloc2log.so"

LD_PRELOAD=$PATH_SO \
          $PATH_ROOT/bin/linux/alloctest \
          2>&1




#LD_PRELOAD=$PATH_SO \
#          $1


