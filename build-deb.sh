#!/bin/sh
#
# build-deb.sh -- package tetrisplus as a Debian/Ubuntu .deb
#
#   ./build-deb.sh              -> dist/tetrisplus_<version>_<arch>.deb
#   ./build-deb.sh 1.2.0        -> build that version instead
#
# Anyone on Ubuntu or Debian can then install it system-wide with:
#
#   sudo apt install ./dist/tetrisplus_<version>_<arch>.deb
#
# ...which puts `tetrisplus` in /usr/bin and pulls in libncurses6 as a dependency.
#
# Like install.sh, this exists because the project has no Makefile on purpose
# (see CLAUDE.md).

set -eu

PROG=tetrisplus
PKG=tetrisplus
HOMEPAGE='https://github.com/aacanadaa/tetrisplus'
MAINTAINER='suoim <suoim@users.noreply.github.com>'

SRC_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
SRC=$SRC_DIR/tetris.c
OUT_DIR=$SRC_DIR/dist

# The default release version. Bump this when cutting a release; it should
# match the git tag, because the .deb version and the tag are the same number.
version=${1:-1.3.0}
arch=$(dpkg --print-architecture)

# As in install.sh: no -std=c99, because tetris.c needs clock_gettime and
# struct timespec, which strict ISO C99 hides.
CC=${CC:-gcc}
CFLAGS=${CFLAGS:--O2 -Wall -Wextra}

say() { printf '%s\n' "$*"; }
die() { printf 'build-deb.sh: %s\n' "$*" >&2; exit 1; }

for tool in dpkg-deb gzip; do
    command -v "$tool" >/dev/null 2>&1 || die "$tool is required but not installed"
done
command -v "$CC" >/dev/null 2>&1 || die "no C compiler found ($CC)"

[ -f "$SRC" ] || die "cannot find tetris.c next to this script"
[ -f "$SRC_DIR/packaging/$PROG.6" ] || die "cannot find packaging/$PROG.6"

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT HUP INT TERM

root=$stage/${PKG}_${version}_${arch}
mkdir -p "$root/DEBIAN" \
         "$root/usr/bin" \
         "$root/usr/share/man/man6" \
         "$root/usr/share/doc/$PKG"

say "Building $PROG $version for $arch"

# shellcheck disable=SC2086  # $CFLAGS is a flag list and must word-split.
"$CC" $CFLAGS -o "$root/usr/bin/$PROG" "$SRC" -lncurses ||
    die "the build failed (see the compiler output above)"
chmod 0755 "$root/usr/bin/$PROG"

# -n keeps the original mtime out of the archive so rebuilds are reproducible.
gzip -9nc "$SRC_DIR/packaging/$PROG.6" > "$root/usr/share/man/man6/$PROG.6.gz"
cp "$SRC_DIR/README.md" "$root/usr/share/doc/$PKG/README.md"
if [ -f "$SRC_DIR/README.en.md" ]; then
    cp "$SRC_DIR/README.en.md" "$root/usr/share/doc/$PKG/README.en.md"
fi
cp "$SRC_DIR/LICENSE"   "$root/usr/share/doc/$PKG/copyright"

# Installed-Size is in KiB and must be estimated before control exists, since
# control itself is not part of the installed size.
installed_size=$(du -sk "$root/usr" | cut -f1)

cat > "$root/DEBIAN/control" <<EOF
Package: $PKG
Version: $version
Architecture: $arch
Maintainer: $MAINTAINER
Depends: libc6, libncurses6, libtinfo6
Section: games
Priority: optional
Installed-Size: $installed_size
Homepage: $HOMEPAGE
Description: Tetris for the terminal
 A complete Tetris game for the Ubuntu terminal, written in C against ncurses.
 Single translation unit, no build system, and no dependencies beyond
 libncurses.
 .
 Includes a game menu, four game modes (Marathon, Sprint, Ultra and Expert)
 and a persistent per-mode arcade high score table.
 .
 Run it by typing "$PROG".
EOF

mkdir -p "$OUT_DIR"
deb=$OUT_DIR/${PKG}_${version}_${arch}.deb

# --root-owner-group makes every path root:root without needing fakeroot or an
# actual root shell.
dpkg-deb --root-owner-group --build "$root" "$deb" >/dev/null ||
    die "dpkg-deb failed"

say ""
say "Built $deb"
say ""
dpkg-deb --info "$deb" | sed 's/^/  /'
say ""
say "Install it with:"
say "  sudo apt install ./${deb#"$SRC_DIR"/}"
