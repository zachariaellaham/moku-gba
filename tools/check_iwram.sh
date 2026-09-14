#!/usr/bin/env bash
# The GBA's stacks live at the top of IWRAM, under whatever the linker has already put there.
# Nothing reports an overflow: the stack simply grows down into .data and the game misbehaves in
# ways that look like a slow or stuck AI. This check fails the build before that can ship.
#
# It caught a real one: a single 64-bit division in a test command pulled libgcc's
# __aeabi_uldivmod (3 KB) into IWRAM, left 3.3 KB for the stacks, and the deep search started
# corrupting itself on 19x19.
set -u

elf="${1:-moku.elf}"
min_free="${IWRAM_MIN_FREE:-5120}"
readelf="${READELF:-${DEVKITARM:-/opt/devkitpro/devkitARM}/bin/arm-none-eabi-readelf}"

[ -f "$elf" ] || { echo "check_iwram: no ELF at $elf" >&2; exit 2; }

used=$("$readelf" -S "$elf" | python3 -c '
import sys
end = 0
for line in sys.stdin:
    f = line.split()
    if len(f) < 7 or f[1] not in (".iwram", ".bss", ".data"):
        continue
    try:
        addr, size = int(f[3], 16), int(f[5], 16)
    except ValueError:
        continue
    if 0x03000000 <= addr < 0x03008000:
        end = max(end, addr + size)
print(end - 0x03000000 if end else 0)
')
top=32768

if [ "${used:-0}" -le 0 ]; then
    echo "check_iwram: could not read the IWRAM sections of $elf with $readelf" >&2
    exit 2
fi

free=$(( top - used ))
printf 'IWRAM: %d of %d bytes static, %d free for the stacks\n' "$used" "$top" "$free"

if [ "$free" -lt "$min_free" ]; then
    echo "check_iwram: only $free bytes left for the stacks (minimum $min_free)." >&2
    echo "  Something new is living in IWRAM. 64-bit division, float helpers and long long" >&2
    echo "  arithmetic all pull kilobytes of libgcc in; use 32-bit maths instead." >&2
    exit 1
fi

exit 0
