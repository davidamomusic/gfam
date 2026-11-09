#!/bin/sh
# Builds and runs the host tests (the same DSP and UI code as the ROM, compiled with gcc for the PC).
#   sh tools/run_tests.sh
# The tests with checks print how many passed and exit non-zero on a failure; the others are smoke tests
# (they must run to the end without crashing; their numbers are for reading).
cd "$(dirname "$0")/.." || exit 1
mkdir -p build
CC="${CC:-gcc}"
FLAGS="-DHOST -O2 -Wall -Wno-unused-function -Wno-unused-variable"
SRC="src/synth.c src/params.c src/fx.c"
fail=0

run() {          # run NAME "extra sources"
    name=$1; extra=$2
    if ! $CC $FLAGS -o build/$name test/$name.c $SRC $extra -lm 2>build/$name.log; then
        echo "BUILD FAIL  $name (see build/$name.log)"; fail=1; return
    fi
    if ./build/$name >build/$name.out 2>&1; then
        echo "ok          $name  $(grep -i -E 'pass|ok|checks' build/$name.out | tail -1)"
    else
        echo "FAIL        $name (see build/$name.out)"; fail=1
    fi
}

run host_buttons ""          # includes src/ui.c itself
run host_klock   "src/ui.c"
run host_pat     "src/ui.c"
run host_patch   "src/ui.c"
run host_rnd     ""
run host_fxrnd   ""
run host_kick    ""
run host_fx      ""
run host_boot    "src/ui.c"

# No Nintendo material: the logo field of every ROM in build/ and releases/ must be empty (see NOTICE.md)
make -s >/dev/null 2>&1
for rom in build/GFAM-v*.gba releases/GFAM-v*.gba; do
    [ -f "$rom" ] || continue
    if python3 -c "import sys; b=open(sys.argv[1],'rb').read(); sys.exit(1 if any(b[4:160]) else 0)" "$rom"; then
        echo "ok          no Nintendo logo in $rom"
    else
        echo "FAIL        $rom has data in the logo field 0x04..0x9F: do not publish it"; fail=1
    fi
done

[ $fail = 0 ] && echo "all host tests ran fine" || echo "SOME TESTS FAILED"
exit $fail
