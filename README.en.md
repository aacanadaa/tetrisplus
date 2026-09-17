<div align="center">

<a href="https://github.com/aacanadaa/tetrisplus">
  <img src="logos/logo-transparent.png" alt="tetrisplus" width="160">
</a>

<h1>Tetris+ (tetrisplus)</h1>

<p><strong>A complete Tetris game for your terminal &mdash; in C, on ncurses and PDCurses.</strong></p>

<p>
  <a href="README.md">简体中文</a> &nbsp;·&nbsp;
  <a href="README.en.md"><strong>English</strong></a>
</p>

<p>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-PolyForm%20Noncommercial%201.0.0-orange.svg" alt="License: PolyForm Noncommercial 1.0.0"></a>
  <a href="https://github.com/aacanadaa/tetrisplus/actions/workflows/ci.yml"><img src="https://github.com/aacanadaa/tetrisplus/actions/workflows/ci.yml/badge.svg" alt="CI status"></a>
  <a href="tetris.c"><img src="https://img.shields.io/badge/C-C99-00599C.svg?logo=c&logoColor=white" alt="Written in C99"></a>
  <img src="https://img.shields.io/badge/TUI-ncurses%20%2F%20PDCurses-3A7D44.svg?logo=gnu&logoColor=white" alt="Runs on ncurses and PDCurses">
  <img src="https://img.shields.io/badge/platform-Linux%20%7C%20macOS%20%7C%20Windows%20%7C%20BSD-2C2D72.svg" alt="Platform: Linux, macOS, Windows and BSD">
  <img src="https://img.shields.io/badge/packaging-.deb-A80030.svg?logo=debian&logoColor=white" alt="Ships a Debian package">
  <a href="https://snapcraft.io/tetrisplus"><img src="https://img.shields.io/badge/snap-tetrisplus-82BEA0.svg?logo=snapcraft&logoColor=white" alt="Available as a snap"></a>
  <a href="https://ko-fi.com/suoim"><img src="https://img.shields.io/badge/Ko--fi-Support%20me-ff5e5b?logo=kofi&logoColor=white" alt="Support me on Ko-fi"></a>
</p>

</div>

Single file and no build system, with nothing but ncurses on the Unix side and
PDCurses on Windows. Comes with a game menu, a persistent arcade-style high
score table, and a game mode system that is ready for more variants.

**The menu** — pick a mode with Left/Right, pick an action with Up/Down:

```
                    T E T R I S
           ──────────────────────────────

               Mode    <  Marathon  >
 Classic endless tetris. Clear lines, survive, score.

           ──────────────────────────────

                > Play

                  High Scores

                  Quit

  Up/Dn move    L/R mode    Enter select    Q quit
```

**Marathon, mid-game** — the falling piece, the ghost showing where a hard drop
lands, the next-piece preview, and the live counters:

```
┌────────────────────┐  TETRIS
│. . . . . []. . . . │  ──────────────
│. . . [][][]. . . . │
│. . . . . . . . . . │  Score
│. . . . . . . . . . │  164
│. . . . . . . . . . │
│. . . . . . . . . . │  Level
│. . . . . . . . . . │  1
│. . . . . . . . . . │
│. . . . . . . . . . │  Lines
│. . . . . . . . . . │  0
│. . . . . . . . . . │
│. . . . . . . . . . │  Next
│. . . . . . . . . . │  . [][]
│. . . . . . . . . . │  [][].
│. . . . . []. . . . │  L/R   move
│. . . [][][]. . . . │  Up    rotate
│[][][][][][]. . . . │  Dn    soft drop
│[][]. . . []. . . . │  Space hard drop
│[][]. . . [][][][]. │  P     pause
│. [][]. . . . [][][]│  M     menu
└────────────────────┘  Q     quit
```

That lone `[]` a few rows above the stack is the **ghost** — it marks where the
piece lands if you hit Space. It renders dimmed in a real terminal.

## Install

Every route ends the same way: a `tetrisplus` command that works from any
directory.

### From the Snap Store

```sh
sudo snap install tetrisplus
```

Works on any Linux with snapd rather than just the Debian family, and updates
itself. Two things are worth knowing up front.

**The name comes from the store, not from the game.** Snap names are globally
unique, and `terminal-tetris` was already registered there by an unrelated
project, so this one carries the `plus`. The repository, the `.deb` and the
command all follow it. A shorter `tetris` alias would have to be granted by the
store by hand, so it is not there by default.

**The snap keeps its own high score table**, because strict confinement hides
the rest of the filesystem from it. See [High scores](#high-scores) for where
that lands and how to remove it.

### From a release

Download the `.deb` from the
[latest release](https://github.com/aacanadaa/tetrisplus/releases/latest)
and install it. The glob saves you typing the version:

```sh
sudo apt install ./tetrisplus_*_amd64.deb
```

That is the one to hand to somebody else. It puts `tetrisplus` in `/usr/bin` for
every user on the machine, pulls in `libncurses6` automatically, and installs a
man page, so `man tetrisplus` works. Remove it with
`sudo apt remove tetrisplus`.

### From an archive, for macOS and Windows

The same [latest release](https://github.com/aacanadaa/tetrisplus/releases/latest)
carries a prebuilt binary for each of the other two platforms. Neither needs a
compiler, ncurses or anything else installed:

- **macOS** — `tetrisplus_<version>_macos_<arch>.tar.gz`. Untar it and run
  `./tetrisplus`. It is ad-hoc signed by the compiler rather than notarised, so
  if Gatekeeper quarantines a download,
  `xattr -d com.apple.quarantine ./tetrisplus` clears it.
- **Windows** — `tetrisplus_<version>_windows_<arch>.zip`. Unzip it and run
  `tetrisplus.exe`. It is statically linked, so the single `.exe` is the whole
  game.

### From a clone, on this machine

```sh
./install.sh
```

That builds the game and installs it to `/usr/local/bin/tetrisplus`, asking for
sudo only if that directory is not writable by you. To keep it out of the
system entirely:

```sh
./install.sh --user     # installs to ~/.local/bin, never uses sudo
```

The installer checks for a compiler and the curses headers before it starts,
and prints the exact install line if either is missing. The same script works in
an MSYS2 MinGW shell on Windows: it links PDCurses there instead and installs
`tetrisplus.exe` into `$MINGW_PREFIX/bin`, which is already on the Windows
`PATH`.

### Building the package yourself

```sh
./build-deb.sh              # -> dist/tetrisplus_<version>_<arch>.deb
sudo apt install ./dist/tetrisplus_*_amd64.deb
```

In an MSYS2 MinGW shell, the same thing for Windows produces a self-contained
release archive:

```sh
./build-windows.sh          # -> dist/tetrisplus_<version>_windows_<arch>.zip
```

### Uninstall

Match the flags you installed with. The prefix has to line up, or the script
looks in the wrong place and finds nothing:

| How you installed it                        | How to undo it                    |
| ------------------------------------------- | --------------------------------- |
| `sudo snap install tetrisplus`              | `sudo snap remove tetrisplus`     |
| `sudo apt install ./tetrisplus_*.deb`       | `sudo apt remove tetrisplus`      |
| `./install.sh` (system-wide)                | `./install.sh --uninstall`        |
| `./install.sh --user`                       | `./install.sh --user --uninstall` |

The two `install.sh` forms need the clone to still be on disk. If you have since
deleted it, the install is only a single file:

```sh
rm ~/.local/bin/tetrisplus           # --user install
sudo rm /usr/local/bin/tetrisplus    # system-wide install
```

**Your high scores are not part of the install**, so none of the above removes
them — they live outside the repo and survive a reinstall. The snap belongs to
that exception too: `snap remove` does take its table along, because the table
lives inside the snap's own data directory, not yours.

To wipe the rest:

```sh
rm -rf "${XDG_DATA_HOME:-$HOME/.local/share}/tetrisplus"   # .deb and install.sh
rm -rf ~/snap/tetrisplus/current/.local/share/tetrisplus   # snap, if you want it gone separately
```

And none of it is a Makefile: this project deliberately does not have one.

## Requirements

Only needed to build from source — the release archives and installers handle
the rest.

The game is C99 with one thin platform layer. On Unix it is pure POSIX and needs
nothing beyond ncurses; on Windows it is built against PDCurses, which offers the
same API. It compiles clean under `-std=c99 -D_POSIX_C_SOURCE=200809L` with no
GNU or glibc extensions, and uses only long-standing curses calls (`initscr`,
`napms` and the basics), so it is not tied to one library version or one flavour
of Unix.

- **Ubuntu / Debian** — `sudo apt install -y build-essential libncurses-dev`
- **macOS** — `xcode-select --install`; ncurses ships with the system
- **Windows** — install [MSYS2](https://www.msys2.org) and, in a UCRT64 shell,
  `pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-pdcurses`
- **FreeBSD / OpenBSD / NetBSD** — ncurses is in the base system, so there is
  nothing to install

Then build it as shown below. If you only want to play, the release archives
above already contain a binary and need none of this.

> Linux, macOS and Windows are all built by CI. The pty test suite runs on Linux
> and macOS; Windows is compile-checked rather than driven, because a headless
> runner has no console to hand it.

## Build

```sh
gcc tetris.c -o tetrisplus -lncurses                  # Linux, macOS, BSD
gcc tetris.c -o tetrisplus.exe -lpdcurses -lwinmm     # Windows (MSYS2)
```

On Windows, add `-static` to fold PDCurses, libgcc and libwinpthreads into the
`.exe` so it runs on a machine with no MinGW installed — that is what
`build-windows.sh` and the release archive do.

## Run

```sh
./tetrisplus        # Linux, macOS, BSD
tetrisplus.exe      # Windows
```

Your terminal needs to be at least **50 columns x 22 rows**. Most default
80x24 terminal windows are fine. Resize at any time — the board and menus
recentre themselves.

## The menu

The game opens on the menu. Up/Down picks an action, Left/Right switches game
mode, Enter confirms.

- **Play** — start a game (or play again after a game over)
- **High Scores** — the top 10 for the current mode
- **Quit** — leave, restoring your terminal

## Controls

### In the menu

| Key             | Action                                |
| --------------- | ------------------------------------- |
| `Up` / `Down`   | Move between actions                  |
| `Left` / `Right`| Switch game mode                      |
| `Enter`         | Confirm                               |
| `Q`             | Quit                                  |

### In a game

| Key             | Action                                |
| --------------- | ------------------------------------- |
| `Left` / `Right`| Move the piece sideways               |
| `Up`            | Rotate clockwise                      |
| `Down`          | Soft drop (+1 point per cell)         |
| `Space`         | Hard drop (+2 points per cell)        |
| `P`             | Pause / resume                        |
| `M`             | Back to the main menu (abandons the run) |
| `Q`             | Quit to the shell                     |

### When the run ends

A run ends when you top out, or when the mode's objective is met — the panel
reads `GAME OVER`, `CLEARED` or `TIME UP` accordingly.

| Key             | Action                                |
| --------------- | ------------------------------------- |
| `R`             | Play again, same mode                 |
| `M`             | Back to the main menu                 |
| `Q`             | Quit                                  |

### Entering your initials

If your score makes the top 10, you get an arcade-style prompt:

| Key             | Action                                |
| --------------- | ------------------------------------- |
| `A`-`Z`, `0`-`9`| Type into the current slot            |
| `Backspace`     | Delete the previous character         |
| `Enter`         | Save the score                        |
| `Esc`           | Skip — the score is not saved         |

## Game modes

| Mode         | Objective                                                   | Ranked by |
| ------------ | ----------------------------------------------------------- | --------- |
| **Marathon** | Endless. Survive and score.                                 | Score     |
| **Sprint**   | Clear 40 lines as fast as you can.                          | **Time**  |
| **Ultra**    | Two minutes on the clock. Score as much as you can.         | Score     |
| **Expert**   | Endless, but starts at level 10 — gravity opens at 170 ms per step instead of 800. | Score |

Gravity ramps from 800 ms per step down to a floor of 80 ms, and the level rises
every 10 lines, except in Expert which starts partway up that curve.

Sprint ranks by **fastest time**, not highest score — its leaderboard shows a
`TIME` column instead of `SCORE`. A time is only recorded if you actually clear
all 40 lines; bailing out at 30 does not post an unbeatable short time. The HUD
swaps the level readout for a clock in any mode with an objective, counting up
in Sprint and down in Ultra.

Adding a mode is a single row in the `MODES[]` table at the top of `tetris.c` —
the rules engine, the menu, the HUD and the score tables all read their
behaviour from there. Each mode keeps its own high score table.

## High scores

Scores persist between sessions, in the standard location for application data.
On Unix:

```
$XDG_DATA_HOME/tetrisplus/scores
# or, if XDG_DATA_HOME is unset:
~/.local/share/tetrisplus/scores
```

On Windows the same file lives under `%LOCALAPPDATA%`:

```
%LOCALAPPDATA%\tetrisplus\scores
```

The snap is the exception. Strict confinement hides your home directory from
it, so it keeps a table of its own inside its sandbox, and the two never see
each other — a score set under the snap will not show up in a `.deb` install,
or the other way round:

```
~/snap/tetrisplus/current/.local/share/tetrisplus/scores
```

The file is plain text so you can read or back it up easily:

```
# tetrisplus high scores
# <mode> <initials> <score> <level> <lines> <date> [<elapsed_ms>]
marathon SUO 12400 5 42 2026-09-09 0
sprint BOT 13782 5 40 2026-09-09 13061
```

The trailing time field is optional and only meaningful for time-ranked modes —
files written before Sprint existed load fine without it, and rows lacking a
time simply sort last on a time-ranked board.

Only the top 10 are kept per mode. The loader is deliberately forgiving: it
ignores comments, blank lines, malformed rows, and rows for modes that no longer
exist. Initials are forced to three display-safe characters, so hand-editing can
never inject terminal escape sequences into the UI.

**To wipe your scores**, just delete the file:

```sh
rm ~/.local/share/tetrisplus/scores          # Unix
del "%LOCALAPPDATA%\tetrisplus\scores"      # Windows (cmd)
```

If the file cannot be read or written for any reason, the game still plays —
only the score table is lost.

## Scoring

| Lines cleared at once | Points          |
| --------------------- | --------------- |
| 1                     | 100 x level     |
| 2                     | 300 x level     |
| 3                     | 500 x level     |
| 4 (a "tetris")        | 800 x level     |

Soft dropping adds 1 point per cell; hard dropping adds 2.

## Features

- All seven tetrominoes (I, J, L, O, S, T, Z), each in its own colour
- Wall kicks, so rotations work flush against the walls
- A ghost piece showing where a hard drop will land
- A 7-bag randomiser, so you never go long stretches without an I piece
- Next-piece preview, score, level and line counters
- Persistent per-mode top-10 high scores with arcade initials
- A menu and a leaderboard screen
- Non-blocking input on a fixed ~60 fps loop, so gravity is smooth and
  independent of your keypresses
- The terminal is always restored on exit, including on Ctrl-C
- Native on Linux, macOS, Windows and the BSDs from one source file

## Testing

The game is a full-screen TUI, so it can't be exercised in a plain shell — it
needs a pty. `tests/run_tests.sh` sets one up, builds the game, and runs three
suites against it:

```sh
tests/run_tests.sh
```

It covers all four mode HUDs, drop scoring, pause, the terminal-size gate,
score-file parsing, and both end conditions end-to-end (Ultra timing out to
`TIME UP`, Sprint reaching `CLEARED` with a time on the board). Everything it
builds goes to `$TETRIS_WORK` (default `/tmp/tetrisplus-test`), so your
working tree is left alone — the run finishes by verifying that.

Needs `python3` with `venv`. `pyte` is installed into a venv for you. On Linux
the runner unpacks the ncurses headers into `$TETRIS_WORK` rather than needing
root; on macOS it uses the ncurses that ships with the system.

The end conditions that would otherwise take minutes to reach are exercised via
variants built on the fly with a shortened constant — a 3-second Ultra clock, a
one-line Sprint goal. Same code path, different numbers.

CI runs this suite on Linux and macOS, and compile-checks the Windows build with
MSYS2 and PDCurses; see `.github/workflows/ci.yml`. Cutting a release pushes a
tag, and `.github/workflows/release.yml` then builds the `.deb`, the macOS
tarball and the Windows zip and publishes them as one GitHub release.

## Layout

```
tetris.c          the entire game
install.sh        build and install it as `tetrisplus`
build-deb.sh      package it as a .deb
build-windows.sh  package it as a Windows .zip
snap/             package it as a snap
packaging/        the man page
logos/            the project logo
LICENSE           the PolyForm Noncommercial license
tests/            pty test suite and its runner
CLAUDE.md         project memory and conventions
README.md         简体中文 (default)
README.en.md      this file
```

## License

[PolyForm Noncommercial 1.0.0](LICENSE) — source-available, not open source.

You may read, run, modify and share this for any **noncommercial** purpose:
personal use, study, hobby projects, and use by charities, schools, public
research bodies, health and safety organisations and government institutions.
Commercial use is not permitted.

Copies released before this license was adopted were published under the MIT
license, and that grant cannot be withdrawn — those versions stay MIT, and so
does anything already forked from them.
