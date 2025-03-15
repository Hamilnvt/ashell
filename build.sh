#!/bin/zsh

cc -o ashell main.c ashed.c ashell_utils.c -lm -Wall -Wextra -Wswitch-enum && ./ashell
