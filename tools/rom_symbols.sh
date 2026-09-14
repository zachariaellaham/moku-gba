#!/bin/sh
# Dump ELF symbols for the ROM harness: tools/rom_symbols.sh [build/moku.elf] > build/moku.sym
/opt/devkitpro/devkitARM/bin/arm-none-eabi-nm "${1:-build/moku.elf}"
