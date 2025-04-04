#!/bin/zsh

SRC_FOLDER=~/Coding/C/shell/

echo "[BUILD] Compiling..."
cc -o ashell "$SRC_FOLDER"main.c "$SRC_FOLDER"ashed.c "$SRC_FOLDER"ashell_utils.c -lm -Wall -Wextra -Wswitch-enum

echo "[BUILD] Executing..."
./ashell

#if [[ -f "ashell" ]] then
#    echo "[BUILD] Removing executable..."
#    rm ashell
#fi

echo "[BUILD] done"
