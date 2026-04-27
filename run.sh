#!/usr/bin/sh

cc sarma.c -o sarma -g3 #-fsanitize=address -fsanitize=leak

if [ $? -eq 0 ]; then
    ./sarma "$@"
fi
