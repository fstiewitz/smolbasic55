#!/bin/bash
set -e

#cmake-build-debug/smolbasic55-riscv64 $FLAGS $1 $1.S
#riscv64-linux-gnu-gcc $1.S data.c array.c input.c print.c control.c string.c math.c -o $1.bin -lm 2>&1
#qemu-riscv64 -L /usr/riscv64-linux-gnu $1.bin
#rm -f $1.S $1.bin

cmake-build-debug/smolbasic55-amd64 $FLAGS $1 $1.S
gcc -g $1.S data.c array.c input.c print.c control.c string.c math.c -o $1.bin -lm 2>&1
$1.bin
rm -f $1.S $1.bin
