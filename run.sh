#!/bin/bash

gcc *.c -o out

if [ $? -ne 0 ]; then
    exit 1
fi

./out