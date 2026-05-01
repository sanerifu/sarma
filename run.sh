#!/usr/bin/sh

cc sarma.c -std=c11 -o sarma -g3 #-fsanitize=address -fsanitize=leak

if [ $? -eq 0 ]; then
    ./sarma "$@"
fi
