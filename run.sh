#!/usr/bin/sh

cc sarma.c -o sarma -fsanitize=address -fsanitize=leak -g3

if [ $? -eq 0 ]; then
    ./sarma
fi
