#!/bin/sh
#
# build-windows.sh -- build the native Windows `tetrisplus.exe` with PDCurses.
#
#   ./build-windows.sh              -> dist/tetrisplus_<version>_windows_<arch>.zip
#   ./build-windows.sh 1.3.0        -> build that version instead
#
# Run it from an MSYS2 MinGW shell (UCRT64, MINGW64, ...) after installing the
# curses package and zip:
#
#   pacman -S --needed "${MINGW_PACKAGE_PREFIX}pdcurses" zip
#
# Like install.sh and build-deb.sh, this exists because the project has no
# Makefile on purpose (see CLAUDE.md).

set -eu

PROG=tetrisplus
SRC_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SRC=$SRC_DIR/tetris.c
OUT_DIR=$SRC_DIR/dist

say() { printf '%s\n' "$*"; }
die() { printf 'build-windows.sh: %s\n' "$*" >&2; exit 1; }

# The release version lives in exactly one place -- build-deb.sh -- so read it
# back rather than repeating the number here, exactly as the snap does. If the
# line ever moves, fail loudly instead of shipping an archive labelled with a
# guess.
version=${1:-}
if [ -z "$version" ]; then
    version=$(sed -n 's/^version=\${1:-\(.*\)}$/\1/p' "$SRC_DIR/build-deb.sh")
    [ -n "$version" ] || die "cannot read the version from build-deb.sh"
fi

case $(uname -s 2>/dev/null) in
    MINGW*|MSYS*|CYGWIN*) ;;
    *) die "run this from an MSYS2/MinGW shell (this one is $(uname -s 2>/dev/null))" ;;
esac

CC=${CC:-gcc}
CFLAGS=${CFLAGS:--O2 -Wall -Wextra}

command -v "$CC" >/dev/null 2>&1 || die "no C compiler found ($CC)"
command -v zip >/dev/null 2>&1 || die "zip is required (pacman -S zip)"
[ -f "$SRC" ] || die "cannot find tetris.c next to this script"
[ -f "$SRC_DIR/LICENSE" ] || die "cannot find LICENSE next to this script"

# Windows names its architectures differently from uname.
case $(uname -m 2>/dev/null) in
    x86_64|amd64)  arch=x86_64 ;;
    aarch64|arm64) arch=arm64  ;;
    i[3-6]86)      arch=x86    ;;
    *)             arch=$(uname -m 2>/dev/null) ;;
esac

mkdir -p "$OUT_DIR"
exe=$OUT_DIR/$PROG.exe

say "Building $PROG $version for windows-$arch"
# -static folds libpdcurses, libgcc and libwinpthreads into the exe, so the
# archive is one file that runs on a machine with no MinGW installed. winmm is
# PDCursesMod's sound back end and is harmless on the classic PDCurses build.
# shellcheck disable=SC2086  # $CFLAGS is a flag list and must word-split.
"$CC" $CFLAGS -o "$exe" "$SRC" -static -lpdcurses -lwinmm ||
    die "the build failed (see the compiler output above)"

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT HUP INT TERM

cp "$exe" "$stage/$PROG.exe"
cp "$SRC" "$stage/tetris.c"
cp "$SRC_DIR/LICENSE" "$stage/LICENSE"
cp "$SRC_DIR/README.md" "$stage/README.md"
if [ -f "$SRC_DIR/README.en.md" ]; then
    cp "$SRC_DIR/README.en.md" "$stage/README.en.md"
fi

zip=$OUT_DIR/${PROG}_${version}_windows_${arch}.zip
rm -f "$zip"
( cd "$stage" && zip -q -9 "$zip" ./* )

say ""
say "Built $exe"
say "Built $zip"
say ""
say "The .exe is statically linked, so the .zip is a single self-contained"
say "program to hand to somebody else."
