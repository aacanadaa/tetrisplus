#!/usr/bin/env bash
# Build tetrisplus and drive it through a pty.
#
#   tests/run_tests.sh
#
# Idempotent and re-runnable. Build artifacts, the venv and the score tables
# all go to a scratch directory (TETRIS_WORK, default /tmp/tetrisplus-test)
# so nothing here writes into the repository.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC="${1:-$HERE/../tetris.c}"
WORK="${TETRIS_WORK:-/tmp/tetrisplus-test}"
export TETRIS_WORK="$WORK"
mkdir -p "$WORK"

say() { printf '\n\033[1m== %s\033[0m\n' "$*"; }

# ---- 1. a curses toolchain -------------------------------------------------
# macOS and the BSDs ship ncurses with the OS, so there is nothing to fetch.
# On Linux sudo needs a password in some environments, so unpack the dev
# package into a local prefix instead of installing it.
BUILD_FLAGS=""
case $(uname -s 2>/dev/null) in
    Darwin|*BSD)
        CURSES_LIBS="-lncurses"
        ;;
    *)
        PREFIX="$WORK/root"
        if [ ! -f "$PREFIX/usr/include/ncurses.h" ]; then
            say "fetching ncurses headers"
            (cd "$WORK" && apt-get download libncurses-dev >/dev/null 2>&1)
            dpkg -x "$WORK"/libncurses-dev_*.deb "$PREFIX/"
        fi
        # The dev package's multiarch directory is named after the CPU, so take
        # whichever one unpacked rather than hard-coding x86_64.
        LIB=""
        for d in "$PREFIX"/usr/lib/*-linux-gnu; do LIB=$d; break; done
        # libncurses.so in that prefix is a linker script -- INPUT(libncurses.so.6
        # -ltinfo) -- so -ltinfo is required, not optional. See CLAUDE.md.
        BUILD_FLAGS="-I$PREFIX/usr/include -L$LIB"
        CURSES_LIBS="-lncurses -ltinfo"
        ;;
esac

build() {   # build <source.c> <output>
    # shellcheck disable=SC2086  # the flag strings are deliberately word-split.
    gcc -Wall -Wextra "$1" -o "$2" $BUILD_FLAGS $CURSES_LIBS
    echo "   built $(basename "$2") -- no warnings"
}

# ---- 2. the real binary, exactly as shipped --------------------------------
say "building the shipped source"
build "$SRC" "$WORK/tetris"

# ---- 3. variants for end conditions too slow to reach honestly -------------
# Same code path, shorter constants, per CLAUDE.md -- never done by editing the
# repo's copy.
say "building test variants (constants shortened, logic identical)"
python3 - "$SRC" "$WORK" <<'PY'
import re, sys
src_path, out_dir = sys.argv[1], sys.argv[2]
src = open(src_path).read()

# Ultra: 120s clock -> 3s, so TIME UP is reachable in seconds.
i = src.index('{ "ultra", "Ultra",')
j = src.index("0, 120, 0 }", i)
ultra = src[:j] + "0, 3, 0 }" + src[j + len("0, 120, 0 }"):]
assert "0, 120, 0 }" not in ultra
open(out_dir + "/tetris-ultra-short.c", "w").write(ultra)

# Sprint: narrow the board so one piece can complete a row, and shorten the
# goal to 1 line. Spawn x is (BOARD_W - BOX) / 2, so it stays in bounds.
out, n = re.subn(r"#define BOARD_W\s+10", "#define BOARD_W    4", src, count=1)
assert n == 1, "board width not replaced"
i = out.index('{ "sprint", "Sprint",')
j = out.index("80, 40, 0, 1 }", i)
narrow = out[:j] + "80, 1, 0, 1 }" + out[j + len("80, 40, 0, 1 }"):]
open(out_dir + "/tetris-narrow.c", "w").write(narrow)
print("   variants generated")
PY
build "$WORK/tetris-ultra-short.c" "$WORK/tetris-ultra-short"
build "$WORK/tetris-narrow.c" "$WORK/tetris-narrow"

# ---- 4. pyte, in a venv (never system python) ------------------------------
if [ ! -x "$WORK/venv/bin/python" ]; then
    say "creating venv and installing pyte"
    python3 -m venv "$WORK/venv"
    "$WORK/venv/bin/pip" install -q --disable-pip-version-check pyte
fi
PY="$WORK/venv/bin/python"

# ---- 5. the suites ---------------------------------------------------------
rc=0
for suite in test_all.py test_timeup.py test_cleared.py; do
    say "$suite"
    (cd "$HERE" && "$PY" "$HERE/$suite") || rc=1
done

# ---- 6. the shipped source must be untouched -------------------------------
say "checking the shipped source is unmodified"
repo="$(dirname "$SRC")"
if git -C "$repo" diff --quiet HEAD -- "$(basename "$SRC")" 2>/dev/null; then
    echo "   $(basename "$SRC") is unchanged from HEAD"
else
    echo "   WARNING: $(basename "$SRC") differs from HEAD"; rc=1
fi
# The constants the variants shorten must still hold their shipped values.
for pat in "#define BOARD_W    10" "80, 40, 0, 1 }" "0, 120, 0 }"; do
    grep -qF "$pat" "$SRC" || { echo "   WARNING: missing '$pat'"; rc=1; }
done
echo "   shipped constants intact"

say "result"
if [ $rc -eq 0 ]; then echo "   ALL SUITES PASSED"; else echo "   FAILURES -- see above"; fi
exit $rc
