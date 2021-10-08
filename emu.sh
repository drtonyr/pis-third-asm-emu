#!/bin/bash -e

./third2asm.py sys.3rd $* post.3rd | ./asm.py | ./txt2bin > tmp.bin

./emu tmp.bin

exit 0
