#!/usr/bin/env bash
# Runs every tests/rom/scripts/*.txt against dist/moku.gba with the mokurun harness.
#
#   tests/rom/run_all.sh                 # all scripts
#   tests/rom/run_all.sh 02 03           # only scripts whose name contains 02 or 03
#   ROM=build/moku.gba tests/rom/run_all.sh
#
# It builds nothing: run `make -j2 && make dist` first (make dist also writes build/moku.sym,
# which the scripts need to resolve $g_moku_test).
# Screenshots land in tests/rom/out/; PNGs that must be kept live in tests/rom/screenshots/.
set -u

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
root="$(cd "$here/../.." && pwd)"

rom="${ROM:-$root/dist/moku.gba}"
sym="${SYM:-$root/build/moku.sym}"
harness="${HARNESS:-$here/harness/mokurun}"
out="$here/out"

fail() { echo "run_all.sh: $*" >&2; exit 2; }

elf="${ELF:-$root/moku.elf}"
nm="${NM:-${DEVKITARM:-/opt/devkitpro/devkitARM}/bin/arm-none-eabi-nm}"

[ -f "$rom" ] || fail "no ROM at $rom (run: make -j2 && make dist)"
[ -x "$harness" ] || fail "no harness at $harness (run: make -C tests/rom/harness)"

# The scripts address the test block as $g_moku_test, which needs the symbol table. `make dist`
# writes it; regenerate it here when it is missing or older than the ELF so a plain `make` is
# enough.
if [ -f "$elf" ] && { [ ! -s "$sym" ] || [ "$elf" -nt "$sym" ]; }; then
    if [ -x "$nm" ]; then
        mkdir -p "$(dirname "$sym")"
        "$nm" "$elf" > "$sym" || fail "cannot read symbols from $elf"
        echo "run_all.sh: regenerated $sym from $elf"
    fi
fi

[ -s "$sym" ] || fail "no symbols at $sym (run: make dist, or set ELF=<path to moku.elf>)"

# Scripts say `symbols build/moku.sym`, so run them from the repo root.
cd "$root" || fail "cannot cd to $root"
mkdir -p "$out"

scripts=()

if [ $# -gt 0 ]; then
    for filter in "$@"; do
        for script in "$here"/scripts/*"$filter"*.txt; do
            [ -f "$script" ] && scripts+=("$script")
        done
    done
else
    for script in "$here"/scripts/*.txt; do
        [ -f "$script" ] && scripts+=("$script")
    done
fi

[ ${#scripts[@]} -gt 0 ] || fail "no scripts matched"

passed=0
failed=0
failed_names=()

for script in "${scripts[@]}"; do
    name="$(basename "$script" .txt)"
    log="$out/$name.log"
    start=$(date +%s)

    if "$harness" "$rom" "$script" > "$log" 2>&1; then
        elapsed=$(( $(date +%s) - start ))
        frames="$(sed -n 's/.*STATS frames=\([0-9]*\).*/\1/p' "$log" | tail -1)"
        echo "PASS $name (${elapsed}s${frames:+, $frames frames})"
        passed=$((passed + 1))
    else
        elapsed=$(( $(date +%s) - start ))
        echo "FAIL $name (${elapsed}s)"
        sed 's/^/     | /' "$log" | tail -12
        failed=$((failed + 1))
        failed_names+=("$name")
    fi
done

echo "----"
echo "$passed passed, $failed failed"

if [ $failed -gt 0 ]; then
    echo "failed: ${failed_names[*]}"
    exit 1
fi

exit 0
