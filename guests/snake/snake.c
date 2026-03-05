#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <time.h>
#include "portator.h"

#define W 30
#define H 20
#define MAX_SNAKE (W * H)

/* Arrow key codes (after reading ESC [ ) */
#define KEY_UP    1000
#define KEY_DOWN  1001
#define KEY_LEFT  1002
#define KEY_RIGHT 1003

static int sx[MAX_SNAKE], sy[MAX_SNAKE];
static int slen;
static int dx, dy;
static int fx, fy;
static int score;
static int game_over;

static struct termios orig_termios;

static void raw_mode(void) {
    struct termios t;
    tcgetattr(STDIN_FILENO, &orig_termios);
    t = orig_termios;
    t.c_lflag &= ~(ICANON | ECHO);
    t.c_cc[VMIN] = 0;
    t.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
}

static void restore_term(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    printf("\033[0m");    /* reset colors */
    printf("\033[?25h");  /* show cursor */
    printf("\033[%d;1H\n", H + 4);
}

static int read_key(void) {
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) != 1) return -1;
    if (c == 27) {
        unsigned char seq[2];
        if (read(STDIN_FILENO, &seq[0], 1) != 1) return 27;
        if (read(STDIN_FILENO, &seq[1], 1) != 1) return 27;
        if (seq[0] == '[') {
            if (seq[1] == 'A') return KEY_UP;
            if (seq[1] == 'B') return KEY_DOWN;
            if (seq[1] == 'C') return KEY_RIGHT;
            if (seq[1] == 'D') return KEY_LEFT;
        }
        return 27;
    }
    return c;
}

static void place_food(void) {
    int i, ok;
    do {
        fx = rand() % W;
        fy = rand() % H;
        ok = 1;
        for (i = 0; i < slen; i++) {
            if (sx[i] == fx && sy[i] == fy) { ok = 0; break; }
        }
    } while (!ok);
}

static void init(void) {
    slen = 3;
    dx = 1; dy = 0;
    sx[0] = W / 2;     sy[0] = H / 2;
    sx[1] = W / 2 - 1; sy[1] = H / 2;
    sx[2] = W / 2 - 2; sy[2] = H / 2;
    score = 0;
    game_over = 0;
    srand(time(NULL));
    place_food();
}

static void draw(void) {
    int x, y, i;
    /* Each cell can be up to ~20 bytes with color codes, plus borders use
       3-byte UTF-8 box chars. 16KB is plenty. */
    char buf[16384];
    int pos = 0;

    pos += sprintf(buf + pos, "\033[H");

    /* top border: dark gray */
    pos += sprintf(buf + pos, "\033[90m\xe2\x95\x94");  /* ╔ */
    for (x = 0; x < W * 2; x++)
        pos += sprintf(buf + pos, "\xe2\x95\x90");       /* ═ */
    pos += sprintf(buf + pos, "\xe2\x95\x97\033[0m\r\n"); /* ╗ */

    for (y = 0; y < H; y++) {
        pos += sprintf(buf + pos, "\033[90m\xe2\x95\x91\033[0m"); /* ║ */
        for (x = 0; x < W; x++) {
            int is_food = (x == fx && y == fy);
            int is_head = 0, is_body = 0;
            if (!is_food) {
                for (i = 0; i < slen; i++) {
                    if (sx[i] == x && sy[i] == y) {
                        if (i == 0) is_head = 1;
                        else is_body = 1;
                        break;
                    }
                }
            }
            if (is_food) {
                pos += sprintf(buf + pos, "\033[91m\xe2\x97\x89 \033[0m"); /* ◉ red */
            } else if (is_head) {
                pos += sprintf(buf + pos, "\033[92m\xe2\x96\x88\xe2\x96\x88\033[0m"); /* ██ bright green */
            } else if (is_body) {
                pos += sprintf(buf + pos, "\033[32m\xe2\x96\x93\xe2\x96\x93\033[0m"); /* ▓▓ green */
            } else {
                buf[pos++] = ' ';
                buf[pos++] = ' ';
            }
        }
        pos += sprintf(buf + pos, "\033[90m\xe2\x95\x91\033[0m\r\n"); /* ║ */
    }

    /* bottom border */
    pos += sprintf(buf + pos, "\033[90m\xe2\x95\x9a");  /* ╚ */
    for (x = 0; x < W * 2; x++)
        pos += sprintf(buf + pos, "\xe2\x95\x90");       /* ═ */
    pos += sprintf(buf + pos, "\xe2\x95\x9d\033[0m\r\n"); /* ╝ */

    pos += sprintf(buf + pos,
        " \033[93mScore: %d\033[0m   "
        "\033[90mArrows/WASD to move, Q to quit\033[0m\r\n", score);

    write(STDOUT_FILENO, buf, pos);
}

static void step(void) {
    int nx, ny, i;

    nx = sx[0] + dx;
    ny = sy[0] + dy;

    if (nx < 0 || nx >= W || ny < 0 || ny >= H) {
        game_over = 1;
        return;
    }

    for (i = 0; i < slen; i++) {
        if (sx[i] == nx && sy[i] == ny) {
            game_over = 1;
            return;
        }
    }

    if (nx == fx && ny == fy) {
        slen++;
        score += 10;
        for (i = slen - 1; i > 0; i--) {
            sx[i] = sx[i - 1];
            sy[i] = sy[i - 1];
        }
        sx[0] = nx;
        sy[0] = ny;
        place_food();
    } else {
        for (i = slen - 1; i > 0; i--) {
            sx[i] = sx[i - 1];
            sy[i] = sy[i - 1];
        }
        sx[0] = nx;
        sy[0] = ny;
    }
}

int main(void) {
    char ver[64];

    raw_mode();
    printf("\033[?25l");  /* hide cursor */
    printf("\033[2J");    /* clear screen */

    if (portator_version(ver, sizeof(ver)) > 0) {
        printf("\033[H\033[36mRunning on %s\033[0m", ver);
        fflush(stdout);
        usleep(800000);
    }

    init();

    while (!game_over) {
        int k = read_key();
        if (k == 'q' || k == 'Q') break;
        if (k == 'w' || k == 'W' || k == KEY_UP)    { if (dy != 1)  { dx = 0;  dy = -1; } }
        if (k == 's' || k == 'S' || k == KEY_DOWN)   { if (dy != -1) { dx = 0;  dy = 1;  } }
        if (k == 'a' || k == 'A' || k == KEY_LEFT)   { if (dx != 1)  { dx = -1; dy = 0;  } }
        if (k == 'd' || k == 'D' || k == KEY_RIGHT)  { if (dx != -1) { dx = 1;  dy = 0;  } }

        step();
        draw();
        usleep(100000);
    }

    restore_term();
    if (game_over)
        printf("\033[91mGame Over!\033[0m Score: \033[93m%d\033[0m\n", score);
    return 0;
}
