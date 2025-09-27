#!/bin/bash

sizes=(10 50 100) # MB

for s in "${sizes[@]}"; do
    fname="test${s}MB.bin"
    dd if=/dev/zero of=$fname bs=1M count=$s status=none

    echo "Transferring $fname..."
    /usr/bin/time -f "Time: %E, CPU: %P" ./client -f $fname
    echo
done