#!/usr/bin/env python3
"""Write a synthetic signature to a FIFO standing in for the Topaz pad.

Usage:
    sudo mkfifo /dev/topaz && sudo chmod 666 /dev/topaz
    ./main.py &
    ./fakepad.py [device]

Packets only reach a signature field once that field is active, so the
signing prompt must be clicked first. Writes 300 points, well over the
MIN_SIG_POINTS threshold, followed by a pen lift so a second run starts a
new stroke instead of connecting to this one.
"""
import math
import sys

PATH = sys.argv[1] if len(sys.argv) > 1 else "/dev/topaz"


def packet(x, y):
    xr, yr = 6 * x + 500, 6 * y + 350
    return bytes([0, 0, xr % 127, xr // 127, yr % 127, yr // 127, 0, 0])


with open(PATH, "wb", buffering=0) as f:
    for i in range(300):
        f.write(packet(i, 42 + int(30 * math.sin(i / 12))))
    f.write(bytes([0, 0x80, 0, 0, 0, 0, 0, 0]))
