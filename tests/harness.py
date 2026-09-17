"""Drive the real tetris binary through a pty and read the screen back.

Follows the recipe in the project's CLAUDE.md: pty + TIOCSWINSZ, pyte to decode,
and a terminfo with no `rep` capability (pyte cannot decode REP, so it renders
collapsed borders and misreports columns). The child runs under LC_ALL=C so
ncurses emits the DEC line-drawing set (which pyte passes through as plain
`q`/`x`) on every platform, rather than UTF-8 box characters whose multi-byte
sequences could also straddle a read() boundary.
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

        # Multi-byte characters can straddle two read() calls, so decode
        # incrementally rather than per chunk -- otherwise a split sequence
        # becomes a sprinkling of U+FFFD.
        self.decoder = codecs.getincrementaldecoder("utf-8")("replace")

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
        return [row.rstrip() for row in self.screen.display]

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
