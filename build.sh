#!/bin/sh
set -xe
gcc main.c -Wall -Wextra -O2 -g -lm -lraylib -lfftw3 -o visualizer
