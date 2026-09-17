"""Drive the real tetris binary through a pty and read the screen back.

Follows the recipe in the project's CLAUDE.md: pty + TIOCSWINSZ, pyte to decode,
and a terminfo with no `rep` capability (pyte cannot decode REP, so it renders
collapsed borders and misreports columns). The child runs under LC_ALL=C so
ncurses draws ACS from the local terminfo rather than as UTF-8 -- but that
terminfo differs between Linux and macOS, so the line-drawing bytes are
normalised to the ASCII sentinels the checks use. See _CP437_ACS below.
"""
import codecs
import curses
import errno
import fcntl
import os
import pty
import select
import signal
import struct
import sys
import termios
import time

import pyte

COLS, ROWS = 80, 24

# The checks below read the frame's vertical border and use `x` as its sentinel
# (and ignore the horizontal rule). Which bytes ncurses emits for ACS depends on
# the local terminfo database: Linux spells the border ASCII `x`, while macOS
# uses the 8-bit CP437 box bytes -- invalid UTF-8, so a UTF-8 decode turns them
# into U+FFFD. Under the C locale the stream is ASCII plus at most those PC
# glyphs, so decoding as CP437 is exact on both platforms (ASCII is identical),
# and the glyphs are then normalised to the same sentinels.
_GLYPH_MAP = str.maketrans({"\u2502": "x", "\u2503": "x", "\u2551": "x",
                            "|": "x", "\u2500": "q", "\u2501": "q",
                            "\u2550": "q"})


def pick_term(preferred):
    """Return the first terminfo entry this machine actually has.

    `linux` is the ideal one here -- it has no `rep` capability, which pyte
    cannot decode -- but it is not installed everywhere: macOS ships a much
    smaller terminfo database. Fall back to other REP-free entries rather than
    dying inside setupterm, and only use xterm as a last resort.
    """
    last = None
    for candidate in (preferred, "vt100", "xterm"):
        try:
            curses.setupterm(term=candidate, fd=-1)
            return candidate
        except curses.error as e:                   # terminfo entry absent
            last = e
    raise RuntimeError("no usable terminfo entry (%s)" % last)


class Harness:
    def __init__(self, binary, cols=COLS, rows=ROWS, term="linux", data_home=None):
        self.cols, self.rows = cols, rows
        self.screen = pyte.Screen(cols, rows)
        self.stream = pyte.Stream(self.screen)
        self.child = None

        # Decode incrementally: a multi-byte sequence can straddle two read()
        # calls, and per-chunk decoding would shred it.
        self.decoder = codecs.getincrementaldecoder("cp437")("replace")

        # Resolve the terminal before forking: the child's TERM has to name the
        # entry we will actually decode with, or its ncurses aborts at startup.
        term = pick_term(term)
        self.term = term

        pid, fd = pty.fork()
        if pid == 0:                                    # child
            env = dict(os.environ)
            env["TERM"] = term
            env["LINES"], env["COLUMNS"] = str(rows), str(cols)
            # Force the C locale: ncurses then draws ACS with the terminfo
            # line-drawing set instead of UTF-8, which is what pyte decodes.
            env["LC_ALL"] = env["LANG"] = "C"
            if data_home:
                env["XDG_DATA_HOME"] = data_home
            os.execve(binary, [binary], env)
        self.child, self.fd = pid, fd

        fcntl.ioctl(fd, termios.TIOCSWINSZ,
                    struct.pack("HHHH", rows, cols, 0, 0))

        curses.setupterm(term=term, fd=fd)
        # TERM=linux has no `kent`, and is missing other keys on some systems,
        # so fall back to the standard ANSI forms rather than sending None.
        defaults = {"left": b"\x1b[D", "right": b"\x1b[C",
                    "up": b"\x1b[A", "down": b"\x1b[B", "enter": b"\r"}
        self.keys = {name: (curses.tigetstr(cap) or defaults[name])
                     for name, cap in (("left", "kcub1"), ("right", "kcuf1"),
                                       ("up", "kcuu1"), ("down", "kcud1"),
                                       ("enter", "kent"))}

    def pump(self, seconds=0.4):
        """Read output into pyte, answering cursor-position queries."""
        deadline = time.time() + seconds
        while time.time() < deadline:
            r, _, _ = select.select([self.fd], [], [], 0.05)
            if not r:
                continue
            try:
                data = os.read(self.fd, 65536)
            except OSError as e:
                if e.errno == errno.EIO:
                    return
                raise
            if not data:
                return
            # ncurses asks where the cursor is and blocks until answered.
            if b"\x1b[6n" in data:
                reply = "\x1b[%d;%dR" % (
                    self.screen.cursor.y + 1, self.screen.cursor.x + 1)
                os.write(self.fd, reply.encode())
            self.stream.feed(self.decoder.decode(data))

    def send(self, name_or_bytes, settle=0.5):
        seq = self.keys.get(name_or_bytes, name_or_bytes)
        if isinstance(seq, str):
            seq = seq.encode()
        os.write(self.fd, seq)
        self.pump(settle)

    def text(self):
        return [row.rstrip().translate(_GLYPH_MAP) for row in self.screen.display]

    def dump(self, label=""):
        if label:
            print("--- %s ---" % label)
        for i, row in enumerate(self.text()):
            print("%2d|%s" % (i, row.replace(" ", "·")))

    def close(self):
        if self.child is None:
            return
        try:
            os.write(self.fd, b"q")
            self.pump(0.3)
        except OSError:
            pass
        for _ in range(20):                             # bounded reap
            pid, status = os.waitpid(self.child, os.WNOHANG)
            if pid:
                break
            time.sleep(0.05)
        else:
            os.kill(self.child, signal.SIGKILL)         # backstop
            os.waitpid(self.child, 0)
        self.child = None
        os.close(self.fd)
