/*
 * tetris.c -- A complete Tetris for the terminal.
 *
 * Built on ncurses everywhere it exists, and on PDCurses on Windows, which
 * exposes the same curses API under a plain <curses.h>. The two platforms are
 * kept apart by the #ifdef _WIN32 block below and a handful of tiny shims --
 * everything else in this file is shared, unmodified source.
 *
 * Build (Linux / macOS / BSD):  gcc tetris.c -o tetrisplus -lncurses
 * Build (Windows / MSYS2):      gcc tetris.c -o tetrisplus.exe -lpdcurses
 * Run:                          ./tetrisplus   (or tetrisplus.exe)
 *
 * Controls:
 *   Left / Right   move the falling piece
 *   Up             rotate clockwise (with wall kicks)
 *   Down           soft drop  (+1 point per cell)
 *   Space          hard drop  (+2 points per cell)
 *   P              pause / resume
 *   Q              quit
 *   R              restart (on the game over screen)
 *
 * Single-file ANSI C. The terminal is always restored via endwin(), including
 * on Ctrl-C and on abnormal exit (atexit handler).
 */

#ifdef _WIN32
/* Windows: PDCurses, plus the Win32 pieces ncurses would otherwise provide.
 * _CRT_SECURE_NO_WARNINGS has to be defined before any system header, or MSVC
 * turns the C99 stdio calls into warnings. */
#  define _CRT_SECURE_NO_WARNINGS 1
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN 1
#  endif
/* PDCurses installs the curses API as <curses.h>; the MSYS2 package ships the
 * very same header as <pdcurses.h> instead. Ask the compiler which one it can
 * actually see, so the source builds on either without a -I flag. */
#  if defined(__has_include)
#    if __has_include(<pdcurses.h>)
#      include <pdcurses.h>
#    elif __has_include(<curses.h>)
#      include <curses.h>
#    else
#      include <curses.h>
#    endif
#  else
#    include <curses.h>
#  endif
#  include <windows.h>            /* GetTickCount64() for the frame clock */
#  include <direct.h>             /* _mkdir()                            */
#  include <process.h>            /* _getpid()                           */
#  define tetris_mkdir(path, mode) _mkdir(path)
#  define TETRIS_BG COLOR_BLACK   /* PDCurses has no "default colour" -1 */
#else
#  include <ncurses.h>
#  include <unistd.h>
#  define tetris_mkdir(path, mode) mkdir(path, mode)
#  define TETRIS_BG (-1)          /* the terminal's own background       */
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#define BOARD_W    10          /* playfield width, in cells            */
#define BOARD_H    20          /* playfield height, in cells           */
#define BOX         4          /* every tetromino lives in a 4x4 box   */
#define NUM_PIECES  7          /* I J L O S T Z                        */
#define NUM_ROTS    4          /* four quarter-turns per piece         */

#define MIN_COLS   50          /* board (22) + panel (16) + margins, and
                                * wide enough for the menu footer and the
                                * leaderboard columns without clipping     */
#define MIN_LINES  22

/* Colour pairs. 1..7 are the tetrominoes, in piece-index order. */
enum {
    PAIR_I = 1, PAIR_J, PAIR_L, PAIR_O, PAIR_S, PAIR_T, PAIR_Z,
    PAIR_FRAME, PAIR_LABEL, PAIR_TEXT, PAIR_OVER
};

/* Spawn orientations, each inside a 4x4 bounding box.  Rotations 1..3 are
 * derived from these at start-up by rotating the box a quarter turn. */
static const char *SHAPE_SRC[NUM_PIECES][BOX] = {
    { "....", "XXXX", "....", "...." },  /* I -- light blue  */
    { "X...", "XXX.", "....", "...." },  /* J -- dark blue   */
    { "..X.", "XXX.", "....", "...." },  /* L -- yellow      */
    { ".XX.", ".XX.", "....", "...." },  /* O -- white       */
    { ".XX.", "XX..", "....", "...." },  /* S -- green       */
    { ".X..", "XXX.", "....", "...." },  /* T -- magenta     */
    { "XX..", ".XX.", "....", "...." }   /* Z -- red         */
};

/* shape[piece][rotation][row][col] -- 1 when the cell is filled. */
static int shape[NUM_PIECES][NUM_ROTS][BOX][BOX];

/* Wall-kick candidates, tried in order when a rotation would collide.
 * Keeps rotations feeling responsive right up against the walls. */
static const int KICKS[][2] = {
    { 0,  0}, {-1,  0}, { 1,  0}, {-2,  0}, { 2,  0},
    { 0, -1}, {-1, -1}, { 1, -1}, {-2, -1}, { 2, -1}
};
#define NUM_KICKS ((int)(sizeof KICKS / sizeof KICKS[0]))

/* A game mode. Adding a new one is a new row in MODES[] below -- the rules
 * engine reads its numbers from here rather than hard-coding them. */
typedef struct {
    const char *id;             /* stable key used in the score file      */
    const char *name;           /* display name                           */
    const char *blurb;          /* one-line description for the menu      */
    int start_level;
    int lines_per_level;
    int base_gravity_ms;        /* drop interval at the starting level     */
    int gravity_step_ms;        /* shaved off per level gained             */
    int min_gravity_ms;         /* floor, however high the level climbs    */
    int goal_lines;             /* 0 = endless; otherwise clear this many  */
    int time_limit_sec;         /* 0 = untimed                             */
    int rank_by_time;           /* 1 = leaderboard sorts by fastest time   */
} GameMode;

/* A mode is just a row here. Anything that varies between modes belongs in
 * this struct -- do not special-case a mode id anywhere else in the file. */
static const GameMode MODES[] = {
    { "marathon", "Marathon",
      "Classic endless tetris. Clear lines, survive, score.",
      1, 10, 800, 70, 80, 0, 0, 0 },

    { "sprint", "Sprint",
      "Clear 40 lines as fast as you can. Ranked by time.",
      1, 10, 800, 70, 80, 40, 0, 1 },

    { "ultra", "Ultra",
      "Two minutes. Score as much as you can before time runs out.",
      1, 10, 800, 70, 80, 0, 120, 0 },

    { "expert", "Expert",
      "Starts at level 10 and stays fast. For people who like pain.",
      10, 10, 800, 70, 80, 0, 0, 0 },
};
#define NUM_MODES ((int)(sizeof MODES / sizeof MODES[0]))

static int mode_is_timed(const GameMode *m)
{
    return m->goal_lines > 0 || m->time_limit_sec > 0;
}

typedef struct {
    const GameMode *mode;         /* ruleset in play                       */
    int board[BOARD_H][BOARD_W];  /* 0 = empty, else a colour-pair index   */
    int piece;                    /* active piece index                    */
    int rot;                      /* active rotation, 0..3                 */
    int x, y;                     /* top-left of the active 4x4 box        */
    int next;                     /* next piece index                      */
    int score, level, lines;
    long last_drop;               /* CLOCK_MONOTONIC ms of last gravity step */
    long elapsed_ms;              /* play time so far, excluding pauses    */
    long last_frame;              /* for accumulating elapsed_ms           */
    int running, paused;
    int to_menu;                  /* asked for the menu, rather than a quit */
    int over;                     /* the run is finished, for any reason   */
    int cleared;                  /* ...because the line goal was reached  */
    int timed_out;                /* ...because the clock ran out          */
} Game;

/* Screen geometry, recomputed every frame so resizes are picked up. */
typedef struct {
    int fx, fy;   /* board frame (border) origin   */
    int ix, iy;   /* board interior origin (0,0)   */
    int px, py;   /* side panel origin             */
} Layout;

static volatile sig_atomic_t g_quit = 0;

static void on_signal(int sig)
{
    (void)sig;
    g_quit = 1;
}

/* Guarantees the terminal is handed back in a usable state. */
static void cleanup(void)
{
    if (!isendwin())
        endwin();
}

static long now_ms(void)
{
#ifdef _WIN32
    /* Milliseconds since boot, monotonic and 64-bit, so the same
     * wrap-around reasoning as CLOCK_MONOTONIC applies. */
    return (long)GetTickCount64();
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long)ts.tv_sec * 1000L + ts.tv_nsec / 1000000L;
#endif
}

/* ------------------------------------------------------------------ setup */

static void init_shapes(void)
{
    for (int p = 0; p < NUM_PIECES; p++) {
        for (int r = 0; r < BOX; r++)
            for (int c = 0; c < BOX; c++)
                shape[p][0][r][c] = (SHAPE_SRC[p][r][c] == 'X');

        /* Each further quarter-turn is the previous box rotated 90 degrees. */
        for (int rot = 1; rot < NUM_ROTS; rot++)
            for (int r = 0; r < BOX; r++)
                for (int c = 0; c < BOX; c++)
                    shape[p][rot][r][c] = shape[p][rot - 1][BOX - 1 - c][r];
    }
}

static void init_colors(void)
{
    if (!has_colors())
        return;

    start_color();
#ifndef _WIN32
    use_default_colors();     /* POSIX only; PDCurses uses TETRIS_BG instead */
#endif

    init_pair(PAIR_I,     COLOR_CYAN,    TETRIS_BG);
    init_pair(PAIR_J,     COLOR_BLUE,    TETRIS_BG);
    init_pair(PAIR_L,     COLOR_YELLOW,  TETRIS_BG);
    init_pair(PAIR_O,     COLOR_WHITE,   TETRIS_BG);
    init_pair(PAIR_S,     COLOR_GREEN,   TETRIS_BG);
    init_pair(PAIR_T,     COLOR_MAGENTA, TETRIS_BG);
    init_pair(PAIR_Z,     COLOR_RED,     TETRIS_BG);
    init_pair(PAIR_FRAME, COLOR_WHITE,   TETRIS_BG);
    init_pair(PAIR_LABEL, COLOR_CYAN,    TETRIS_BG);
    init_pair(PAIR_TEXT,  COLOR_WHITE,   TETRIS_BG);
    init_pair(PAIR_OVER,  COLOR_RED,     COLOR_BLACK);
}

/* 7-bag randomiser: every piece appears once per bag, so runs of bad luck
 * are bounded and the distribution stays honest. */
static int bag[NUM_PIECES];
static int bag_pos = NUM_PIECES;

static int next_piece(void)
{
    if (bag_pos >= NUM_PIECES) {
        for (int i = 0; i < NUM_PIECES; i++)
            bag[i] = i;
        for (int i = NUM_PIECES - 1; i > 0; i--) {   /* Fisher-Yates */
            int j = rand() % (i + 1);
            int t = bag[i];
            bag[i] = bag[j];
            bag[j] = t;
        }
        bag_pos = 0;
    }
    return bag[bag_pos++];
}

/* -------------------------------------------------------------- game rules */

static int collides(const Game *g, int piece, int rot, int px, int py)
{
    for (int r = 0; r < BOX; r++) {
        for (int c = 0; c < BOX; c++) {
            if (!shape[piece][rot][r][c])
                continue;

            int bx = px + c;
            int by = py + r;

            if (bx < 0 || bx >= BOARD_W || by >= BOARD_H)
                return 1;
            if (by >= 0 && g->board[by][bx])
                return 1;
        }
    }
    return 0;
}

static void spawn(Game *g)
{
    g->piece = g->next;
    g->next  = next_piece();
    g->rot   = 0;
    g->x     = (BOARD_W - BOX) / 2;
    g->y     = 0;

    /* Nowhere to put the new piece: the stack has reached the top. */
    if (collides(g, g->piece, g->rot, g->x, g->y))
        g->over = 1;
}

static int try_move(Game *g, int dx, int dy)
{
    if (collides(g, g->piece, g->rot, g->x + dx, g->y + dy))
        return 0;

    g->x += dx;
    g->y += dy;
    return 1;
}

static void try_rotate(Game *g)
{
    int nr = (g->rot + 1) % NUM_ROTS;

    for (int k = 0; k < NUM_KICKS; k++) {
        int nx = g->x + KICKS[k][0];
        int ny = g->y + KICKS[k][1];

        if (!collides(g, g->piece, nr, nx, ny)) {
            g->rot = nr;
            g->x   = nx;
            g->y   = ny;
            return;
        }
    }
    /* Every kick failed -- the rotation is not possible here. */
}

static void clear_lines(Game *g)
{
    static const int POINTS[5] = { 0, 100, 300, 500, 800 };
    int cleared = 0;

    for (int row = BOARD_H - 1; row >= 0; row--) {
        int full = 1;
        for (int c = 0; c < BOARD_W; c++) {
            if (!g->board[row][c]) {
                full = 0;
                break;
            }
        }
        if (!full)
            continue;

        /* Drop everything above this row down by one. */
        for (int r = row; r > 0; r--)
            memcpy(g->board[r], g->board[r - 1], sizeof g->board[0]);
        memset(g->board[0], 0, sizeof g->board[0]);

        cleared++;
        row++;   /* re-test the row we just refilled */
    }

    if (cleared > 0) {
        g->score += POINTS[cleared] * g->level;
        g->lines += cleared;
        g->level  = g->mode->start_level +
                    g->lines / g->mode->lines_per_level;
    }
}

static void lock_piece(Game *g)
{
    for (int r = 0; r < BOX; r++)
        for (int c = 0; c < BOX; c++)
            if (shape[g->piece][g->rot][r][c]) {
                int bx = g->x + c;
                int by = g->y + r;
                if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W)
                    g->board[by][bx] = g->piece + 1;
            }

    clear_lines(g);
    spawn(g);
}

static void hard_drop(Game *g)
{
    int dist = 0;

    while (!collides(g, g->piece, g->rot, g->x, g->y + 1)) {
        g->y++;
        dist++;
    }

    g->score += dist * 2;
    lock_piece(g);
}

/* Gravity ramps up with level; the curve itself comes from the game mode. */
static int drop_interval_ms(const Game *g)
{
    const GameMode *m = g->mode;
    int ms = m->base_gravity_ms - (g->level - 1) * m->gravity_step_ms;

    return ms < m->min_gravity_ms ? m->min_gravity_ms : ms;
}

static void handle_input(Game *g, int ch, long now)
{
    if (ch == ERR)
        return;

    switch (ch) {
    case 'q': case 'Q':
        g->running = 0;
        break;
    case 'm': case 'M':
        g->to_menu = 1;
        g->running = 0;
        break;
    case 'p': case 'P':
        g->paused = !g->paused;
        g->last_drop = now;      /* don't dump a piece the instant we resume */
        break;
    case KEY_LEFT:
        if (!g->paused) try_move(g, -1, 0);
        break;
    case KEY_RIGHT:
        if (!g->paused) try_move(g, 1, 0);
        break;
    case KEY_UP:
        if (!g->paused) try_rotate(g);
        break;
    case KEY_DOWN:
        if (!g->paused && try_move(g, 0, 1)) {
            g->score += 1;
            g->last_drop = now;
        }
        break;
    case ' ':
        if (!g->paused) hard_drop(g);
        break;
    default:
        break;
    }
}

/* ---------------------------------------------------------------- scores */

#define MAX_SCORES 10
#define INITIALS    3          /* arcade-style, three characters */
#define TABLE_W    46          /* leaderboard column block, one shared edge */

typedef struct {
    char name[INITIALS + 1];
    int  score;
    int  level;
    int  lines;
    long elapsed_ms;           /* 0 when the mode does not time the run */
    char date[11];             /* YYYY-MM-DD */
} ScoreEntry;

typedef struct {
    ScoreEntry e[MAX_SCORES];
    int n;
} ScoreTable;

static ScoreTable tables[NUM_MODES];

static int find_mode(const char *id)
{
    for (int i = 0; i < NUM_MODES; i++)
        if (strcmp(MODES[i].id, id) == 0)
            return i;
    return -1;
}

/* Force anything read off disk into three safe display characters, so a
 * hand-edited score file can never inject escape sequences into the UI. */
static void clean_initials(char *dst, const char *src)
{
    int n = 0;

    for (; src[n] && n < INITIALS; n++) {
        char c = src[n];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 32);
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
            c = '-';
        dst[n] = c;
    }
    while (n < INITIALS)
        dst[n++] = '-';
    dst[INITIALS] = '\0';
}

static void today(char *buf, size_t len)
{
    time_t t = time(NULL);
    struct tm *lt = localtime(&t);

    if (lt)
        strftime(buf, len, "%Y-%m-%d", lt);
    else
        snprintf(buf, len, "0000-00-00");
}

/* Where the score table lives.
 *
 * POSIX:   $XDG_DATA_HOME/tetrisplus, else ~/.local/share/tetrisplus.
 * Windows: %LOCALAPPDATA%/tetrisplus, else %APPDATA%/tetrisplus.
 *
 * Renamed from "terminal-tetris" with the project. That deliberately orphans
 * every score table saved under the old name, so this is not a string to
 * change again: it is the on-disk identity of the table, and moving it costs
 * every player their high scores. */
static int scores_dir(char *buf, size_t len)
{
    int n;

#ifdef _WIN32
    /* LOCALAPPDATA is the roaming-free application-data root, which is where
     * a save file belongs; APPDATA is the fallback on ancient systems. Both
     * are absolute and already exist, so only the last component is created.
     * Forward slashes work with the Win32 file APIs and keep mkpath() and the
     * rest of the shared code path-agnostic. */
    const char *base = getenv("LOCALAPPDATA");
    if (base == NULL || base[0] == '\0')
        base = getenv("APPDATA");
    if (base == NULL || base[0] == '\0')
        return -1;

    n = snprintf(buf, len, "%s/tetrisplus", base);
#else
    const char *base = getenv("XDG_DATA_HOME");
    const char *home;

    if (base && base[0] == '/')
        n = snprintf(buf, len, "%s/tetrisplus", base);
    else if ((home = getenv("HOME")) != NULL && home[0] == '/')
        n = snprintf(buf, len, "%s/.local/share/tetrisplus", home);
    else
        return -1;
#endif

    return (n > 0 && (size_t)n < len) ? 0 : -1;
}

static int scores_path(char *buf, size_t len)
{
    char dir[512];
    int n;

    if (scores_dir(dir, sizeof dir) != 0)
        return -1;

    n = snprintf(buf, len, "%s/scores", dir);
    return (n > 0 && (size_t)n < len) ? 0 : -1;
}

/* mkdir -p, one component at a time. Failures are ignored on purpose: if we
 * cannot write scores the game should still be playable. */
static void mkpath(const char *path)
{
    char tmp[512];
    size_t len = strlen(path);

    if (len == 0 || len >= sizeof tmp)
        return;

    memcpy(tmp, path, len + 1);

    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/')
            continue;
        *p = '\0';
        tetris_mkdir(tmp, 0700);
        *p = '/';
    }
    tetris_mkdir(tmp, 0700);
}

/* Mode-aware ordering: fastest time wins on sprint-style modes, highest score
 * everywhere else. An entry with no recorded time sorts last. */
static int entry_better(const ScoreEntry *a, const ScoreEntry *b, int by_time)
{
    if (by_time) {
        if (a->elapsed_ms <= 0)
            return 0;
        if (b->elapsed_ms <= 0)
            return 1;
        return a->elapsed_ms < b->elapsed_ms;
    }
    return a->score > b->score;
}

/* Keeps each mode's table sorted, best first, capped at MAX_SCORES. Reading
 * in any order gives the same result, so a hand-edited file is safe. */
static void insert_score(int mi, const ScoreEntry *e)
{
    ScoreTable *t = &tables[mi];
    int by_time = MODES[mi].rank_by_time;
    int pos = t->n;

    for (int i = 0; i < t->n; i++) {
        if (entry_better(e, &t->e[i], by_time)) {
            pos = i;
            break;
        }
    }
    if (pos >= MAX_SCORES)
        return;                    /* didn't make the cut */

    if (t->n < MAX_SCORES)
        t->n++;

    for (int i = t->n - 1; i > pos; i--)
        t->e[i] = t->e[i - 1];
    t->e[pos] = *e;
}

static int qualifies(int mi, const ScoreEntry *e)
{
    const ScoreTable *t = &tables[mi];
    int by_time = MODES[mi].rank_by_time;

    if (by_time) {
        if (e->elapsed_ms <= 0)
            return 0;              /* never finished, so nothing to rank */
    } else if (e->score <= 0) {
        return 0;
    }

    if (t->n < MAX_SCORES)
        return 1;

    return entry_better(e, &t->e[MAX_SCORES - 1], by_time);
}

static void load_scores(void)
{
    char path[512], line[256];
    FILE *f;

    memset(tables, 0, sizeof tables);

    if (scores_path(path, sizeof path) != 0)
        return;
    if ((f = fopen(path, "r")) == NULL)
        return;                    /* first run: no file yet, all good */

    while (fgets(line, sizeof line, f)) {
        char mode[32], raw[64], date[64];
        ScoreEntry e;
        int mi, got;

        if (line[0] == '#' || line[0] == '\n')
            continue;

        memset(&e, 0, sizeof e);

        /* The trailing time field is optional, so files written before timed
         * modes existed still load -- they just read as "no time recorded". */
        got = sscanf(line, "%31s %63s %d %d %d %63s %ld",
                     mode, raw, &e.score, &e.level, &e.lines, date,
                     &e.elapsed_ms);
        if (got < 6)
            continue;              /* malformed line: skip it */

        if ((mi = find_mode(mode)) < 0)
            continue;              /* score for a mode that no longer exists */

        clean_initials(e.name, raw);
        snprintf(e.date, sizeof e.date, "%.10s", date);
        insert_score(mi, &e);
    }

    fclose(f);
}

static void save_scores(void)
{
    char dir[512], path[512];
    FILE *f;

    if (scores_dir(dir, sizeof dir) != 0)
        return;
    mkpath(dir);

    if (scores_path(path, sizeof path) != 0)
        return;
    if ((f = fopen(path, "w")) == NULL)
        return;

    fprintf(f, "# tetrisplus high scores\n");
    fprintf(f, "# <mode> <initials> <score> <level> <lines> <date> [<elapsed_ms>]\n");

    for (int m = 0; m < NUM_MODES; m++)
        for (int i = 0; i < tables[m].n; i++) {
            const ScoreEntry *e = &tables[m].e[i];
            fprintf(f, "%s %s %d %d %d %s %ld\n", MODES[m].id, e->name,
                    e->score, e->level, e->lines, e->date, e->elapsed_ms);
        }

    fclose(f);
}

/* ---------------------------------------------------------------- drawing */

static void compute_layout(Layout *L)
{
    int frame_w = BOARD_W * 2 + 2;    /* two characters per cell + borders */
    int frame_h = BOARD_H + 2;
    int panel_w = 16;
    int total_w = frame_w + panel_w;

    L->fx = (COLS - total_w) / 2;
    L->fy = (LINES - frame_h) / 2;
    if (L->fx < 0) L->fx = 0;
    if (L->fy < 0) L->fy = 0;

    L->ix = L->fx + 1;                /* inside the border */
    L->iy = L->fy + 1;
    L->px = L->fx + frame_w + 2;
    L->py = L->fy;
}

static void draw_frame(int fx, int fy, int w, int h, int pair)
{
    attron(COLOR_PAIR(pair));
    mvaddch(fy, fx, ACS_ULCORNER);
    mvhline(fy, fx + 1, ACS_HLINE, w - 2);
    mvaddch(fy, fx + w - 1, ACS_URCORNER);
    mvaddch(fy + h - 1, fx, ACS_LLCORNER);
    mvhline(fy + h - 1, fx + 1, ACS_HLINE, w - 2);
    mvaddch(fy + h - 1, fx + w - 1, ACS_LRCORNER);
    mvvline(fy + 1, fx, ACS_VLINE, h - 2);
    mvvline(fy + 1, fx + w - 1, ACS_VLINE, h - 2);
    attroff(COLOR_PAIR(pair));
}

static void draw_cell(int x, int y, int pair, int filled)
{
    if (filled) {
        attron(COLOR_PAIR(pair) | A_BOLD);
        mvaddstr(y, x, "[]");
        attroff(COLOR_PAIR(pair) | A_BOLD);
    } else {
        attron(A_DIM);
        mvaddstr(y, x, ". ");
        attroff(A_DIM);
    }
}

static void draw_board(const Game *g, const Layout *L)
{
    /* Settled stack. */
    for (int r = 0; r < BOARD_H; r++)
        for (int c = 0; c < BOARD_W; c++)
            draw_cell(L->ix + c * 2, L->iy + r, g->board[r][c],
                      g->board[r][c] != 0);

    if (g->over)
        return;

    /* Ghost: where the piece lands on a hard drop. */
    int gy = g->y;
    while (!collides(g, g->piece, g->rot, g->x, gy + 1))
        gy++;

    if (gy != g->y) {
        for (int r = 0; r < BOX; r++)
            for (int c = 0; c < BOX; c++)
                if (shape[g->piece][g->rot][r][c]) {
                    int bx = g->x + c;
                    int by = gy + r;
                    if (by < 0 || by >= BOARD_H || bx < 0 || bx >= BOARD_W)
                        continue;
                    attron(COLOR_PAIR(g->piece + 1) | A_DIM);
                    mvaddstr(L->iy + by, L->ix + bx * 2, "[]");
                    attroff(COLOR_PAIR(g->piece + 1) | A_DIM);
                }
    }

    /* Active piece. */
    for (int r = 0; r < BOX; r++)
        for (int c = 0; c < BOX; c++) {
            if (!shape[g->piece][g->rot][r][c])
                continue;
            int bx = g->x + c;
            int by = g->y + r;
            if (by < 0 || by >= BOARD_H || bx < 0 || bx >= BOARD_W)
                continue;
            draw_cell(L->ix + bx * 2, L->iy + by, g->piece + 1, 1);
        }
}

static void draw_preview(const Game *g, int x, int y)
{
    int p = g->next;
    int minr = BOX, maxr = -1, minc = BOX, maxc = -1;

    /* Trim the 4x4 box down to the cells the piece actually uses. */
    for (int r = 0; r < BOX; r++)
        for (int c = 0; c < BOX; c++)
            if (shape[p][0][r][c]) {
                if (r < minr) minr = r;
                if (r > maxr) maxr = r;
                if (c < minc) minc = c;
                if (c > maxc) maxc = c;
            }

    if (maxr < 0)
        return;

    for (int r = minr; r <= maxr; r++)
        for (int c = minc; c <= maxc; c++)
            draw_cell(x + (c - minc) * 2, y + (r - minr), p + 1,
                      shape[p][0][r][c]);
}

static void panel_label(int y, int x, const char *s)
{
    attron(COLOR_PAIR(PAIR_LABEL));
    mvaddstr(y, x, s);
    attroff(COLOR_PAIR(PAIR_LABEL));
}

static void panel_field(int *y, int x, const char *label, const char *value)
{
    panel_label(*y, x, label);
    attron(COLOR_PAIR(PAIR_TEXT) | A_BOLD);
    mvaddstr(*y + 1, x, value);
    attroff(COLOR_PAIR(PAIR_TEXT) | A_BOLD);
    *y += 3;
}

static void panel_row(int *y, int x, const char *s)
{
    attron(COLOR_PAIR(PAIR_TEXT));
    mvaddstr(*y, x, s);
    attroff(COLOR_PAIR(PAIR_TEXT));
    *y += 1;
}

/* m:ss.t, clamped at zero so a countdown never shows a negative clock.
 * Tenths because sprint times are routinely decided by under a second. */
static void fmt_time(char *buf, size_t len, long ms)
{
    if (ms < 0)
        ms = 0;
    snprintf(buf, len, "%ld:%02ld.%ld",
             ms / 60000, (ms / 1000) % 60, (ms % 1000) / 100);
}

static void draw_panel(const Game *g, const Layout *L)
{
    const GameMode *m = g->mode;
    char buf[32];
    int x = L->px, y = L->py;
    int cy;

    attron(COLOR_PAIR(PAIR_LABEL) | A_BOLD);
    mvaddstr(y, x, "TETRIS");
    attroff(COLOR_PAIR(PAIR_LABEL) | A_BOLD);

    attron(COLOR_PAIR(PAIR_FRAME));
    mvhline(y + 1, x, ACS_HLINE, 14);
    attroff(COLOR_PAIR(PAIR_FRAME));

    cy = y + 3;

    snprintf(buf, sizeof buf, "%d", g->score);
    panel_field(&cy, x, "Score", buf);

    /* Timed and objective modes trade the level readout for a clock: level
     * matters less when the run is bounded by time or a line target. */
    if (mode_is_timed(m)) {
        fmt_time(buf, sizeof buf,
                 m->time_limit_sec > 0
                     ? (long)m->time_limit_sec * 1000L - g->elapsed_ms
                     : g->elapsed_ms);
        panel_field(&cy, x, "Time", buf);
    } else {
        snprintf(buf, sizeof buf, "%d", g->level);
        panel_field(&cy, x, "Level", buf);
    }

    if (m->goal_lines > 0)
        snprintf(buf, sizeof buf, "%d/%d", g->lines, m->goal_lines);
    else
        snprintf(buf, sizeof buf, "%d", g->lines);
    panel_field(&cy, x, "Lines", buf);

    panel_label(cy, x, "Next");
    draw_preview(g, x, cy + 1);
    cy += 3;                    /* one row tighter, so the list still ends
                                 * level with the board's bottom border */

    panel_row(&cy, x, "L/R   move");
    panel_row(&cy, x, "Up    rotate");
    panel_row(&cy, x, "Dn    soft drop");
    panel_row(&cy, x, "Space hard drop");
    panel_row(&cy, x, "P     pause");
    panel_row(&cy, x, "M     menu");
    panel_row(&cy, x, "Q     quit");
}

/* A filled, bordered box anywhere on the screen. */
static void draw_panel_box(int x, int y, int w, int h, int pair)
{
    for (int r = 0; r < h; r++)
        for (int c = 0; c < w; c++) {
            attron(COLOR_PAIR(pair));
            mvaddch(y + r, x + c, ' ');
            attroff(COLOR_PAIR(pair));
        }

    draw_frame(x, y, w, h, pair);
}

static void panel_center(int x, int y, int w, const char *s, int pair, int attrs)
{
    int tx = x + (w - (int)strlen(s)) / 2;

    if (tx < x + 1)
        tx = x + 1;

    attron(COLOR_PAIR(pair) | attrs);
    mvaddstr(y, tx, s);
    attroff(COLOR_PAIR(pair) | attrs);
}

/* A message box centred over the board, sized to its longest line but never
 * wider than the board itself. First line renders bold. */
static void board_panel(const Layout *L, const char *const *lines, int n, int pair)
{
    int w = 18, h = n + 2;
    int x, y;

    for (int i = 0; i < n; i++) {
        int need = (int)strlen(lines[i]) + 4;
        if (need > w)
            w = need;
    }
    if (w > BOARD_W * 2)
        w = BOARD_W * 2;

    x = L->ix + (BOARD_W * 2 - w) / 2;
    y = L->iy + (BOARD_H - h) / 2;

    draw_panel_box(x, y, w, h, pair);

    for (int i = 0; i < n; i++)
        panel_center(x, y + 1 + i, w, lines[i], pair,
                     i == 0 ? A_BOLD : A_NORMAL);
}

static void draw(const Game *g)
{
    Layout L;

    erase();

    if (COLS < MIN_COLS || LINES < MIN_LINES) {
        attron(COLOR_PAIR(PAIR_OVER) | A_BOLD);
        mvaddstr(LINES / 2, 0, "Terminal too small");
        attroff(COLOR_PAIR(PAIR_OVER) | A_BOLD);
        mvprintw(LINES / 2 + 1, 0, "Need %dx%d, have %dx%d", MIN_COLS,
                 MIN_LINES, COLS, LINES);
        refresh();
        return;
    }

    compute_layout(&L);

    draw_frame(L.fx, L.fy, BOARD_W * 2 + 2, BOARD_H + 2, PAIR_FRAME);
    draw_board(g, &L);
    draw_panel(g, &L);

    if (g->paused) {
        const char *lines[] = { "PAUSED", "press P" };
        board_panel(&L, lines, 2, PAIR_OVER);
    } else if (g->over) {
        const char *title = g->cleared   ? "CLEARED"
                          : g->timed_out ? "TIME UP"
                                         : "GAME OVER";
        const char *lines[] = { title, "R retry  M menu", "Q quit" };
        board_panel(&L, lines, 3, g->cleared ? PAIR_S : PAIR_OVER);
    }

    refresh();
}

/* --------------------------------------------------------------- screens */

typedef enum { SCR_MENU, SCR_GAME, SCR_SCORES, SCR_QUIT } Screen;

static void put_str(int y, int x, const char *s, int pair, int attrs)
{
    attron(COLOR_PAIR(pair) | attrs);
    mvaddstr(y, x, s);
    attroff(COLOR_PAIR(pair) | attrs);
}

static void center_text(int y, const char *s, int pair, int attrs)
{
    int x = (COLS - (int)strlen(s)) / 2;

    if (x < 0)
        x = 0;
    put_str(y, x, s, pair, attrs);
}

static void center_rule(int y, int w, int pair)
{
    int x = (COLS - w) / 2;

    if (x < 0)
        x = 0;

    attron(COLOR_PAIR(pair));
    mvhline(y, x, ACS_HLINE, w);
    attroff(COLOR_PAIR(pair));
}

/* Every screen shares this guard so a shrunken window never draws garbage. */
static int screen_too_small(void)
{
    if (COLS >= MIN_COLS && LINES >= MIN_LINES)
        return 0;

    erase();
    center_text(LINES / 2, "Terminal too small", PAIR_OVER, A_BOLD);

    char buf[80];
    snprintf(buf, sizeof buf, "Need %dx%d, have %dx%d",
             MIN_COLS, MIN_LINES, COLS, LINES);
    center_text(LINES / 2 + 1, buf, PAIR_OVER, A_NORMAL);
    refresh();
    return 1;
}

/* Arcade initials entry. Returns 1 if the player confirmed, 0 if they skipped. */
static int run_initials_entry(const ScoreEntry *e, char *out)
{
    char buf[INITIALS + 1];
    int pos = 0, saved = 0;

    memset(buf, '-', INITIALS);
    buf[INITIALS] = '\0';

    while (!g_quit && !saved) {
        char line[128];
        int ch, w = 40, x, y, done = 0;

        if (screen_too_small()) {
            napms(100);
            continue;
        }

        x = (COLS - w) / 2;
        y = (LINES - 12) / 2;
        if (x < 0) x = 0;
        if (y < 0) y = 0;

        erase();
        draw_panel_box(x, y, w, 12, PAIR_OVER);
        panel_center(x, y + 1, w, "NEW HIGH SCORE", PAIR_OVER, A_BOLD);

        snprintf(line, sizeof line, "Score   %d", e->score);
        panel_center(x, y + 3, w, line, PAIR_OVER, A_NORMAL);
        snprintf(line, sizeof line, "Level   %d", e->level);
        panel_center(x, y + 4, w, line, PAIR_OVER, A_NORMAL);
        snprintf(line, sizeof line, "Lines   %d", e->lines);
        panel_center(x, y + 5, w, line, PAIR_OVER, A_NORMAL);

        panel_center(x, y + 7, w, "Enter your initials", PAIR_OVER, A_NORMAL);

        int sx = x + (w - (INITIALS * 4 - 1)) / 2;
        for (int i = 0; i < INITIALS; i++) {
            chtype a = (i == pos) ? A_REVERSE : A_NORMAL;
            attron(COLOR_PAIR(PAIR_OVER) | A_BOLD | a);
            mvaddch(y + 9, sx + i * 4, (chtype)buf[i]);
            attroff(COLOR_PAIR(PAIR_OVER) | A_BOLD | a);
        }

        panel_center(x, y + 10, w, "A-Z 0-9 type  Enter save  Esc skip",
                     PAIR_OVER, A_NORMAL);
        refresh();

        while ((ch = getch()) != ERR) {
            if (ch == 27) {                       /* Esc -- skip */
                done = 1;
                break;
            }
            if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                saved = 1;
                break;
            }
            if (ch == KEY_BACKSPACE || ch == 127 || ch == 8) {
                if (pos > 0)
                    buf[--pos] = '-';
                continue;
            }
            if (ch >= 'a' && ch <= 'z')
                ch -= 32;
            if ((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
                buf[pos] = (char)ch;
                if (pos < INITIALS - 1)
                    pos++;
            }
        }

        if (done)
            break;
        napms(16);
    }

    if (saved)
        memcpy(out, buf, INITIALS + 1);

    return saved;
}

/* Main menu. Returns the screen to go to next. */
static Screen run_menu(int *mode)
{
    enum { ACT_PLAY, ACT_SCORES, ACT_QUIT, NUM_ACTIONS };
    static const char *const ACTIONS[NUM_ACTIONS] = { "Play", "High Scores", "Quit" };
    int sel = ACT_PLAY;

    while (!g_quit) {
        char buf[160];
        int ch, top, bx;

        if (screen_too_small()) {
            napms(100);
            continue;
        }

        top = LINES / 2 - 7;
        if (top < 0)
            top = 0;

        erase();
        center_text(top, "T E T R I S", PAIR_LABEL, A_BOLD);
        center_rule(top + 1, 30, PAIR_FRAME);

        snprintf(buf, sizeof buf, "Mode    <  %s  >", MODES[*mode].name);
        center_text(top + 3, buf, PAIR_TEXT, A_BOLD);
        snprintf(buf, sizeof buf, "%.64s", MODES[*mode].blurb);
        center_text(top + 4, buf, PAIR_TEXT, A_NORMAL);

        center_rule(top + 6, 30, PAIR_FRAME);

        bx = (COLS - 24) / 2;
        if (bx < 0)
            bx = 0;

        for (int i = 0; i < NUM_ACTIONS; i++) {
            snprintf(buf, sizeof buf, "  %s %s", i == sel ? ">" : " ", ACTIONS[i]);
            put_str(top + 8 + i * 2, bx, buf,
                    i == sel ? PAIR_LABEL : PAIR_TEXT,
                    i == sel ? A_BOLD : A_NORMAL);
        }

        center_text(LINES - 2, "Up/Dn move    L/R mode    Enter select    Q quit",
                    PAIR_FRAME, A_NORMAL);
        refresh();

        while ((ch = getch()) != ERR) {
            if (ch == 'q' || ch == 'Q')
                return SCR_QUIT;

            if (ch == KEY_UP)
                sel = (sel + NUM_ACTIONS - 1) % NUM_ACTIONS;
            else if (ch == KEY_DOWN)
                sel = (sel + 1) % NUM_ACTIONS;
            else if (ch == KEY_LEFT)
                *mode = (*mode + NUM_MODES - 1) % NUM_MODES;
            else if (ch == KEY_RIGHT)
                *mode = (*mode + 1) % NUM_MODES;
            else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                if (sel == ACT_PLAY)
                    return SCR_GAME;
                else if (sel == ACT_SCORES)
                    return SCR_SCORES;
                else
                    return SCR_QUIT;
            }
        }
        napms(16);
    }
    return SCR_QUIT;
}

/* The top-10 table for one mode, switchable with left/right. */
static Screen run_scores(int *mode)
{
    int m = *mode;

    while (!g_quit) {
        const ScoreTable *t = &tables[m];
        char buf[160];
        int ch, tx, by_time;

        if (screen_too_small()) {
            napms(100);
            continue;
        }

        erase();
        snprintf(buf, sizeof buf, "HIGH SCORES  --  %s", MODES[m].name);
        center_text(2, buf, PAIR_LABEL, A_BOLD);
        center_rule(3, 46, PAIR_FRAME);

        /* One shared left edge for the header and every row: centring each
         * line on its own would shear the columns apart. */
        tx = (COLS - TABLE_W) / 2;
        if (tx < 0)
            tx = 0;

        /* Time-ranked modes swap the score column for a clock -- the score
         * is not what a sprint is measuring. Column width is unchanged. */
        by_time = MODES[m].rank_by_time;

        snprintf(buf, sizeof buf, "%2s   %-4s  %7s   %3s   %5s   %s",
                 "#", "NAME", by_time ? "TIME" : "SCORE", "LV", "LINES", "DATE");
        put_str(5, tx, buf, PAIR_FRAME, A_BOLD);

        if (t->n == 0) {
            put_str(7, tx, "  No scores yet -- go set one.", PAIR_TEXT, A_NORMAL);
        } else {
            for (int i = 0; i < t->n; i++) {
                const ScoreEntry *e = &t->e[i];
                char metric[16];

                if (by_time)
                    fmt_time(metric, sizeof metric, e->elapsed_ms);
                else
                    snprintf(metric, sizeof metric, "%d", e->score);

                snprintf(buf, sizeof buf,
                         "%2d   %-4s  %7s   %3d   %5d   %s",
                         i + 1, e->name, metric, e->level, e->lines, e->date);
                put_str(7 + i, tx, buf,
                        i == 0 ? PAIR_LABEL : PAIR_TEXT,
                        i == 0 ? A_BOLD : A_NORMAL);
            }
        }

        center_text(LINES - 2, "L/R switch mode    Esc/Q back",
                    PAIR_FRAME, A_NORMAL);
        refresh();

        while ((ch = getch()) != ERR) {
            if (ch == 27 || ch == 'q' || ch == 'Q' ||
                ch == '\n' || ch == '\r')
                return SCR_MENU;
            if (ch == KEY_LEFT)
                m = (m + NUM_MODES - 1) % NUM_MODES;
            else if (ch == KEY_RIGHT)
                m = (m + 1) % NUM_MODES;
        }
        napms(16);
    }
    *mode = m;
    return SCR_QUIT;
}

static Screen run_game_over(const Game *g)
{
    while (!g_quit) {
        int ch;

        draw(g);                    /* final board + GAME OVER panel */

        while ((ch = getch()) != ERR) {
            switch (ch) {
            case 'r': case 'R': return SCR_GAME;
            case 'm': case 'M': return SCR_MENU;
            case 'q': case 'Q': return SCR_QUIT;
            default: break;
            }
        }
        napms(16);
    }
    return SCR_QUIT;
}

/* A mode can end without topping out: clear the line goal, or run out of
 * clock. Both are checked once per unpaused frame. */
static void check_objective(Game *g)
{
    const GameMode *m = g->mode;

    if (m->goal_lines > 0 && g->lines >= m->goal_lines) {
        g->over    = 1;
        g->cleared = 1;
    } else if (m->time_limit_sec > 0 &&
               g->elapsed_ms >= (long)m->time_limit_sec * 1000L) {
        g->over      = 1;
        g->timed_out = 1;
    }
}

static Screen run_game(int mi)
{
    const GameMode *m = &MODES[mi];
    Game g;

    memset(&g, 0, sizeof g);
    g.mode       = m;
    g.level      = m->start_level;
    g.running    = 1;
    g.last_drop  = now_ms();
    g.last_frame = g.last_drop;

    bag_pos = NUM_PIECES;           /* fresh bag every game */
    g.next  = next_piece();
    spawn(&g);

    while (g.running && !g_quit && !g.over) {
        int ch;
        long now;

        /* Drain every pending keypress before ticking the clock. */
        while ((ch = getch()) != ERR)
            handle_input(&g, ch, now_ms());

        if (g_quit)
            break;

        if (screen_too_small()) {
            napms(100);
            continue;
        }

        now = now_ms();

        if (!g.paused) {
            g.elapsed_ms += now - g.last_frame;

            if (now - g.last_drop >= drop_interval_ms(&g)) {
                g.last_drop = now;
                if (!try_move(&g, 0, 1))
                    lock_piece(&g);
            }
            check_objective(&g);
        }
        g.last_frame = now;

        draw(&g);
        napms(16);                  /* ~60 fps */
    }

    /* Leaving mid-run abandons it: no score is recorded either way. */
    if (!g.over)
        return g.to_menu ? SCR_MENU : SCR_QUIT;

    /* Offer a place on the table if this run earned one. */
    {
        ScoreEntry e;

        memset(&e, 0, sizeof e);
        snprintf(e.name, sizeof e.name, "---");
        e.score = g.score;
        e.level = g.level;
        e.lines = g.lines;
        /* Only record a time if the run actually finished: an abandoned
         * sprint should not post an unbeatable short time. */
        e.elapsed_ms = g.cleared ? g.elapsed_ms : 0;
        today(e.date, sizeof e.date);

        if (qualifies(mi, &e)) {
            char name[INITIALS + 1];

            if (run_initials_entry(&e, name)) {
                memcpy(e.name, name, sizeof e.name);
                insert_score(mi, &e);
                save_scores();
            }
        }
    }

    return run_game_over(&g);
}

/* ------------------------------------------------------------------- main */

int main(void)
{
    Screen screen = SCR_MENU;
    int mode = 0;

    if (initscr() == NULL) {
        fprintf(stderr, "tetrisplus: failed to initialise ncurses\n");
        return 1;
    }
    atexit(cleanup);          /* restore the terminal on any exit path */

    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);    /* non-blocking input */
    curs_set(0);

    init_colors();
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

#ifdef _WIN32
    srand((unsigned)time(NULL) ^ (unsigned)_getpid());
#else
    srand((unsigned)time(NULL) ^ (unsigned)getpid());
#endif
    init_shapes();
    load_scores();

    while (screen != SCR_QUIT && !g_quit) {
        switch (screen) {
        case SCR_MENU:   screen = run_menu(&mode);   break;
        case SCR_SCORES: screen = run_scores(&mode); break;
        case SCR_GAME:   screen = run_game(mode);    break;
        case SCR_QUIT:   break;
        }
    }

    endwin();
    return 0;
}
