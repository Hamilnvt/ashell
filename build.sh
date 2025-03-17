#!/bin/zsh

SRC_FOLDER=~/Coding/C/shell/

echo "Compiling..."
cc -o ashell "$SRC_FOLDER"main.c "$SRC_FOLDER"ashed.c "$SRC_FOLDER"ashell_utils.c -lm -Wall -Wextra -Wswitch-enum

echo "Executing..."
./ashell

echo "Removing executable..."
rm ashell
