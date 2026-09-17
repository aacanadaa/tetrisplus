# CLAUDE.md — tetrisplus

Project memory for Claude Code. Read this before changing anything here.

## Project

A complete Tetris game for the terminal, written in C. It builds against
ncurses on any POSIX system (Linux, macOS and the BSDs) and against PDCurses on
Windows; the two share the whole file apart from the single `#ifdef _WIN32`
block at the top of `tetris.c`.
Deliberately a **single translation unit** — no Makefile, no headers, no
subdirectories. Everything lives in `tetris.c`, currently ~1400 lines.

Features: a game menu, a mode system, and a persistent per-mode arcade high
score table.

## Build

```sh
gcc tetris.c -o tetrisplus -lncurses                  # Linux, macOS, BSD
gcc tetris.c -o tetrisplus.exe -lpdcurses -lwinmm     # Windows (MSYS2 UCRT64)
```

System dependency (Ubuntu/Debian):

```sh
sudo apt update && sudo apt install -y build-essential libncurses-dev
```

## The Windows platform layer

Everything platform-specific lives in the `#ifdef _WIN32` block at the top of
`tetris.c`, and nothing else in the file is conditionally compiled:

- **Curses header.** PDCurses normally installs `<curses.h>`; the MSYS2 package
  ships the same API as `<pdcurses.h>`. `__has_include` picks whichever the
  toolchain can see, so no `-I` flag is needed.
- **Timing.** `now_ms()` returns `GetTickCount64()` on Windows instead of
  `clock_gettime(CLOCK_MONOTONIC)`, and `srand()` seeds on `_getpid()`.
- **Colours.** PDCurses has no "default background" `-1`, so `TETRIS_BG` is
  `COLOR_BLACK` there and `-1` elsewhere; `use_default_colors()` is POSIX-only.
- **Paths.** `_mkdir` replaces `mkdir`, and the score table lands in
  `%LOCALAPPDATA%/tetrisplus` (falling back to `%APPDATA%`). Paths use forward
  slashes throughout, which the Win32 APIs accept, so `mkpath()` stays shared.

Do **not** move POSIX-only code into the shared section or reach for a
`#ifdef _WIN32` deeper in the file: the whole point is that the game logic does
not know what platform it is on.

Keep the code **pure POSIX C99** outside that block. The README advertises macOS
and the BSDs, so reaching for a GNU extension silently breaks a supported
platform. The cheap check is to compile with the POSIX feature macro set, which
hides anything glibc-only:

```sh
gcc -std=c99 -D_POSIX_C_SOURCE=200809L -Wall -Wextra tetris.c -o /dev/null -lncurses
```

No `<linux/*>`, no `/proc`, no epoll or inotify, no `strdupa`/`asprintf`-style
extensions. CI covers all three families: the pty suite runs on Linux and macOS,
and Windows is compile-checked with MSYS2 and PDCurses (`ci.yml`). A regression
on a platform CI cannot run will still need a manual build.

There is no Makefile on purpose. If you find yourself wanting one, the change
is out of scope for this project. Installation lives in shell scripts instead:

- `install.sh` — builds and installs the binary as `tetrisplus`. Defaults to
  `/usr/local/bin` (using sudo only when that is not writable), `--user` for
  `~/.local/bin`, `--prefix DIR`, `--uninstall`. **`--uninstall` resolves the
  same prefix as install does**, so it only removes what the matching install
  put there — a `--user` install needs `--user --uninstall`, and a bare
  `--uninstall` will silently find nothing. It prints a hint when that happens,
  but the failure mode is "nothing happened", not an error.
- `build-deb.sh` — produces `dist/tetrisplus_<version>_<arch>.deb`, which
  installs to `/usr/bin` for every user and declares `libncurses6`. This is
  where the release version lives; every other script reads it back from here.
- `build-windows.sh` — runs in an MSYS2 MinGW shell and produces
  `dist/tetrisplus_<version>_windows_<arch>.zip` containing a statically linked
  `tetrisplus.exe`.
- `packaging/tetrisplus.6` — the man page, shipped by the `.deb`.
- `snap/snapcraft.yaml` — packages the same source as the snap **`tetrisplus`**,
  published on the Snap Store. The compile lives in the part's `override-build`
  rather than in a Makefile, for the same reason the rest of this list exists.
  See *Snap packaging* below before touching it.

**Do not add `-std=c99` to any build command.** `now_ms()` calls
`clock_gettime()` and uses `struct timespec`, which glibc only declares when
the POSIX feature macros are in scope. Under strict ISO C99 the build fails
outright. The correct flags are `-O2 -Wall -Wextra`, which stay warning-clean.

The build scripts stage into a temp dir and never touch the score file —
uninstalling does not lose anyone's high scores. `build-deb.sh` and
`build-windows.sh` drop their finished package into `dist/`, which is
gitignored; `install.sh` keeps its binary in a temp dir and never writes into
the repo on its own.

**Note on linking:** the plain `-lncurses` form is correct once
`libncurses-dev` is installed. If you are building against headers extracted
into a userspace prefix (see *Building without root* below), you must also pass
`-ltinfo` explicitly, because `cbreak` and friends live in libtinfo.

### Building without root

`sudo` in this environment needs an interactive password. To build anyway:

```sh
apt-get download libncurses-dev && dpkg -x libncurses-dev_*.deb root/
gcc tetris.c -o tetrisplus -Iroot/usr/include -L<dir-with-libncurses.so> \
    -lncurses -ltinfo
```

The runtime `libncurses.so.6` is usually already present; only the headers and
the `.so` dev symlink are missing.

### Snap packaging

Build it with **`snapcraft pack --destructive-mode`**, not a bare
`snapcraft pack`. The plain form wants LXD, and this machine runs Docker, which
sets the iptables `FORWARD` policy to `DROP` and cuts egress from `lxdbr0` —
the build then dies with `Timed out waiting for networking to be ready`. The
host is Ubuntu 26.04, exactly what `core26` is built from, so destructive mode
is a faithful build. `sudo snap install core26` has to happen first: craft-parts
skips a base that is already present, but runs `snap install` as your user, with
no sudo, when it is missing.

Three things about the snap are load-bearing and easy to break:

- **The name is `tetrisplus`, and it is permanent.** `terminal-tetris` was
  already published by a different project, and a registered snap name can
  never be changed. `tetris+` is not a legal name either — lowercase
  alphanumerics and hyphens only — which is why the `+` lives in the `title`.
  The app key must match the name, or the command stops landing on `PATH`.
- **`libncurses6` is staged on purpose.** core26 ships only the wide
  `libncursesw.so.6`. `tetris.c` draws with `ACS_*` and plain `mvaddstr` and
  never calls `setlocale()`, so it links the narrow library the base does not
  carry. Remove that `stage-packages` entry and the snap builds fine, installs,
  then dies at launch with `libncurses.so.6: cannot open shared object file`.
- **`snap/term-fallback` is not optional.** ncurses reads terminfo only from
  the base, which carries 71 entries and no kitty, alacritty or foot among
  them, so `initscr()` aborts with `cannot initialize terminal type`. The
  wrapper degrades `TERM` to `xterm-256color` only when the entry is missing.
  Shipping a terminfo database does not fix this: `xterm-kitty` is not in
  `ncurses-term` at all.

The version is adopted from `build-deb.sh` rather than repeated, so the file
still holds exactly one copy. Confinement is `strict` with **no plugs**: the
game's only write is the score table, and snapd already points `XDG_DATA_HOME`
inside the sandbox. The consequence is that the snap's scores are separate from
every other install's, and no interface can share them without a hand-reviewed
`personal-files` grant.

**The snap is amd64 only.** Nothing in `snapcraft.yaml` restricts it — the
`amd64` in the filename comes from whatever machine ran `snapcraft pack`, and
`architectures:` is deliberately left out so a build on any host produces a
snap for that host. Getting arm64 as well means letting Launchpad do the
building, which is configured on the web rather than in this repo: on the
snap's page at snapcraft.io, under *Builds*, point it at this GitHub repo and
add an `arm64` entry to the build set. Launchpad then builds and uploads on
every push to `main`, and the store serves whichever architectures exist. A
release cut on an amd64 laptop will not carry arm64 until that is set up.

## Run

```sh
./tetris
```

Needs a terminal of at least **50 columns x 22 rows**. The menu footer and the
leaderboard columns are what set the 50; the board alone would fit in 38. Any
screen that is too small shows a "Terminal too small" notice rather than drawing
garbage.

## Code style rules

- **Single-file ANSI C.** All code stays in `tetris.c`. Do not split it up.
- **4-space indentation.** No tabs.
- C99 declarations. Where a variable is only used in a narrow scope, declare it
  there rather than at the top of the function.
- **Explicit ncurses cleanup on every exit path.** `endwin()` must run whether
  the player quits, dies, or hits Ctrl-C. Enforced two ways: `atexit(cleanup)`
  as the backstop, and a `SIGINT`/`SIGTERM` handler that sets `g_quit` so the
  main loop unwinds normally.
- Section banners (`/* ---- name */`) separate setup, rules, scores, drawing,
  screens, and main.
- Keep comments about *why*, not *what*. The rotation derivation, the
  line-clear compaction, the shared-edge table alignment and the initials
  sanitiser all deserve their existing notes; trivia does not.

## Architecture

### Game modes

`MODES[]` near the top is the single source of truth for mode behaviour — name,
blurb, starting level, lines-per-level, the gravity curve, and any objective
(`goal_lines`, `time_limit_sec`) or ranking rule (`rank_by_time`). There are **no
hard-coded 800 / 70 / 80 / 10 / 40 / 120 constants anywhere else in the file**;
the engine reads them off `Game::mode`. Adding a mode is one row in that table
plus a `blurb`.

The id string is the stable key used in the score file, so **never change an
existing id** — doing so orphans everyone's saved scores.

`mode_is_timed()` really means "has an objective"; the HUD uses it to choose
between a Level readout and a clock. `check_objective()` runs once per unpaused
frame and is the only place a run can end without topping out. It sets `over`
plus either `cleared` or `timed_out`, and `draw()` turns that into the panel
title (`CLEARED` / `TIME UP` / `GAME OVER`).

Time-ranked modes sort **ascending** — fastest first — via `entry_better()`.
`qualifies()` refuses an entry with no recorded time, so an abandoned Sprint can
never post an unbeatable short time. `run_game()` only stamps `elapsed_ms` when
`cleared` is set, which is the other half of that guard.

### Screen state machine

`main()` does essentially nothing: it initialises ncurses, loads scores, then
loops on a `Screen` value (`SCR_MENU`, `SCR_GAME`, `SCR_SCORES`, `SCR_QUIT`),
dispatching to `run_menu()`, `run_game()` or `run_scores()`. Each of those
returns the screen to go to next, so navigation is data, not call nesting.

`run_game()` owns one game: it plays the loop to completion, then offers the
initials prompt if the score qualifies, then hands off to `run_game_over()`.
Retry is just `run_game()` returning `SCR_GAME`, which re-enters it.

Every screen loop follows the same shape: check `screen_too_small()`, `erase()`,
draw, `refresh()`, then drain all pending keypresses with a non-blocking
`getch()` before `napms(16)`. Keep that pattern — it is what makes the ~60 fps
loop and independent gravity work.

### Rendering

- `SHAPE_SRC` holds only the **spawn orientation** of each tetromino. The other
  three rotations are generated at start-up in `init_shapes()`. Never hand-write
  rotation tables.
- The board stores `0` for empty and `piece + 1` for a settled cell, so the
  stored value *is* the ncurses colour-pair index. Intentional.
- `compute_layout()` runs every frame, so resizes are picked up for free.
- `draw_panel_box()` / `panel_center()` / `board_panel()` are the reusable
  panel primitives. `board_panel()` sizes itself to its longest line and is
  capped at the board width.
- **Never centre table rows independently.** The leaderboard uses one shared
  left edge (`TABLE_W`) via `put_str()`; centring each line on its own shears
  the columns apart, because rows differ in length. This bug has been fixed once
  already.
- `center_text()` clamps negative `x` to 0. That is deliberate — clipping beats
  a negative `mvaddstr` writing off-screen.

### High scores

Plain text at `$XDG_DATA_HOME/tetrisplus/scores`, falling back to
`~/.local/share/tetrisplus/scores`. Directory is created `0700`; failures
to create or write are swallowed on purpose, because losing a score table must
never stop the game from being playable.

Line format, one entry per line, comments start with `#`:

```
<mode-id> <initials> <score> <level> <lines> <YYYY-MM-DD> [<elapsed_ms>]
```

The trailing time is **optional on purpose**: files written before timed modes
existed must keep loading, and a row without a time sorts last on a time-ranked
board. Parse it with a `>= 6` check on the `sscanf` return, not `== 7`, and
`memset` the entry first so the field is defined when the conversion is absent.

`insert_score()` maintains a sorted top-`MAX_SCORES`, so reading the file **in
any order** gives the same result. That is what makes a hand-edited file safe.

Parsing is defensive and must stay that way. It skips malformed rows, rows with
too few fields, rows for unknown mode ids, and anything starting with `#`.
`clean_initials()` forces names to three characters from `[A-Z0-9]` (everything
else becomes `-`), which is what stops a hand-edited file from injecting
terminal escape sequences into the UI. Do not relax this.

## Testing

Compiles clean with `-Wall -Wextra`. Please keep it that way:

```sh
gcc -Wall -Wextra tetris.c -o tetrisplus -lncurses
```

`tests/run_tests.sh` builds the game, generates the variants described below,
and runs the three suites against it — 38 checks covering the four mode HUDs,
scoring, pause, size gating, score-file parsing, and both end conditions
end-to-end. Build artifacts, the venv and the score tables all go to
`$TETRIS_WORK` (default `/tmp/tetrisplus-test`), so the runner never writes
into the repo. It finishes by asserting `tetris.c` is unchanged from HEAD and
that the constants the variants shorten still hold their shipped values.

`tests/run_sanitizers.sh` then rebuilds those same three binaries with
AddressSanitizer and UndefinedBehaviorSanitizer and re-runs all three suites
against them. It reuses `$TETRIS_WORK`, so `run_tests.sh` has to run first. Two
details matter there: it asserts `libasan` actually linked before trusting a
green result, since a build the sanitizers never attached to would pass for
entirely the wrong reason; and it sets `detect_leaks=0`, because ncurses holds
allocations until `endwin()` and the exit report is pure noise.

`.github/workflows/ci.yml` runs `run_tests.sh` on every push and pull request on
both Linux and macOS, and `run_sanitizers.sh` on Linux (LeakSanitizer is not
portable). The same workflow also compiles with `-Werror`, separately with
`-D_POSIX_C_SOURCE` to guard the portability the README advertises, and
separately on Windows with MSYS2 + PDCurses. On macOS `run_tests.sh` uses the
system ncurses instead of unpacking a header package, and the harness picks the
first terminfo entry the machine actually has (`linux`, then `vt100`, then
`xterm`) rather than assuming `linux` exists.

The game is a full-screen TUI, so it cannot be run in a plain shell — it needs a
pty. The pattern that works:

- Allocate a pty (`pty.fork()` in Python), set the window size with
  `TIOCSWINSZ`, and drive it with real key sequences from `curses.tigetstr`
  (`kcub1`, `kcuf1`, `kcuu1`, `kcud1`, `kent`).
- Decode the output with `pyte` (installed into a **venv**, never system
  Python) to read back the screen.
- Answer `\x1b[6n` cursor-position queries, or ncurses blocks waiting for a
  reply.
- **Use a terminfo without `rep` (e.g. `TERM=linux` or `vt100`) when checking
  layout.** ncurses uses REP to compress runs of identical characters such as
  box-drawing borders, and pyte does not implement REP — so `TERM=xterm-256color`
  makes pyte render collapsed borders and misreport column positions. This
  produces convincing but entirely fake "bugs".
- Point `XDG_DATA_HOME` at a temp directory so tests never touch real scores.

Two traps worth knowing, both of which produce *convincing fake bugs*:

- **Teardown hangs on the initials prompt.** If a run ends with a qualifying
  score, the game sits waiting for initials. A bare `q` just types a letter into
  it, so the child never exits and an unbounded `waitpid` blocks forever. Send
  `Esc` first to dismiss the prompt, then reap with `WNOHANG` in a bounded loop,
  with `SIGKILL` as the backstop.
- **Reading the active piece has to scan vertically.** The piece spawns at box
  `x = 3`, but gravity can tick between a lock and your read, moving it to
  `y = 1` before you look — so matching only at `y = 0` fails intermittently and
  then keeps failing for the whole fall. Scan `y` offsets 0..8 for the
  rotation-0 pattern. The vertical position is irrelevant anyway: the landing
  row is the same wherever the piece currently sits, so all you need is its
  identity.

Exercising the end conditions needs a player, not just keypresses. A small
greedy placement bot is enough: keep your own board model, read only the piece
identity off the screen, and simulate each rotation/column to score placements
by lines cleared minus bumpiness, holes and max height. One of those cleared
Sprint's full 40 lines, which is what actually proved the goal path end to end.

For an end condition too slow to reach honestly — Ultra's 120-second clock,
which a bot that tops out first never sees — build a variant **in `/tmp`** with
the constant shortened (`sed` the `time_limit_sec` field of that mode's row) and
drive that instead. Same code path, different constant. Afterwards confirm the
shipped source still holds the real value; never test by editing the repo file.

Check by hand after touching rules or screens:

- Rotating flush against each of the four walls (wall kicks).
- Clearing a single line, and a tetris at once.
- Each mode's HUD: Level for Marathon and Expert, a counting-**up** clock plus
  `N/40` for Sprint, a counting-**down** clock for Ultra.
- Sprint ending at exactly 40 lines: `CLEARED` panel, a time written to the
  score file, and a TIME-ranked board with the fastest first.
- Stacking out: the initials prompt appears only if the score qualifies, `Esc`
  skips without saving, and `Enter` persists it.
- The score surviving a full restart — relaunch and check the leaderboard
  reads from disk, not just from memory.
- A deliberately corrupted score file (junk rows, unknown mode, over-long
  initials, raw escape bytes) loading without crashing.

## Releasing

The version lives in exactly one place: the `version=${1:-...}` default at the
top of `build-deb.sh`. Bump it so it matches the git tag, or the `.deb` ships
with a version that disagrees with the release it came from.

Pushing the tag is what cuts the release. `.github/workflows/release.yml` then
builds each platform on its own runner — the `.deb`, a macOS tarball and a
Windows zip — and publishes them as one GitHub release, so there is no
hand-built asset and nothing to upload by hand:

```sh
# bump the default in build-deb.sh and commit that first
./tests/run_tests.sh            # confirms tetris.c is still untouched
git tag -a v<v> -m "..."
git push origin main && git push origin v<v>
gh run watch                    # release.yml builds and publishes
```

`gh release create` marks the newest non-prerelease as Latest automatically, so
there is no need to touch the older one.

The snap is released separately, and takes the same version out of the same
file — nothing to bump here:

```sh
snapcraft pack --destructive-mode
snapcraft upload --release=edge tetrisplus_<v>_amd64.snap
sudo snap refresh tetrisplus --edge && tetrisplus   # check it actually runs
snapcraft release tetrisplus <revision> stable
```

Revisions are numbered by the store, not by us, and they are **not** the
version: uploading the same `<v>` twice produces revisions 1 and 2. Get the
number from the upload output or `snapcraft status tetrisplus`.

Store metadata — including `license:` — is read from the snap's own
`meta/snap.yaml`, so a metadata change only reaches the listing when a new
revision is uploaded and released. Editing `snapcraft.yaml` alone changes
nothing that anyone browsing the store can see.

**Never edit a published revision to "correct" history.** Snap revision 2 says
`license: MIT` because it *was* MIT, and the GitHub releases up to `v1.1.1`
ship an MIT `LICENSE` for the same reason. Those grants cannot be withdrawn.
PolyForm applies to the repository from here on, and to the store from snap
revision 3 onward.

**Never retag, move or delete a published tag** — cut a new version instead.
`v1.0.0` is permanently at `470d71c`, which predates the pty test suite, and
that is fine; it is not a reason to rewrite history.

## Git

Default branch is `main`. Do not commit the `tetrisplus` binary — it is in
`.gitignore` and always will be. The score file lives outside the repo, so it is
never a commit risk.

`main` is protected. Force-pushes and deletions are blocked, and `Build and test`
plus `Installers and package` must pass before a pull request can be merged, so
an outside contribution cannot land on a red build. `enforce_admins` is
deliberately **off**, which keeps the owner's direct pushes working.

That last part is the sharp edge, and it is not obvious: with `enforce_admins`
**on**, `required_status_checks` binds direct pushes too, not just pull-request
merges. A plain `git push origin main` then fails with
`GH006: 2 of 2 required status checks are expected`, because the new commit has
no check run yet, and the only way in becomes a branch, a pull request and a
merge. Turning `enforce_admins` off restores the one-step push while leaving the
gate on incoming pull requests. The cost is that the owner can also bypass the
force-push and deletion blocks — so those guard against accidents and outside
collaborators, not against the owner.

Do **not** enable "Require approvals" — GitHub will not let you approve your own
pull request, so requiring one locks the sole maintainer out of merging.
