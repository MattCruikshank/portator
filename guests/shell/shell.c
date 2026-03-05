#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>
#include "portator.h"

static char cwd[4096] = "zip";

/*───────────────────────────────────────────────────────────────────────────╗
│ History                                                                    │
╚───────────────────────────────────────────────────────────────────────────*/

#define HISTORY_MAX 64

static char history[HISTORY_MAX][256];
static int history_count;

static void history_add(const char *line) {
    if (!line[0]) return;
    /* Don't duplicate the last entry */
    if (history_count > 0 &&
        strcmp(history[history_count - 1], line) == 0)
        return;
    if (history_count < HISTORY_MAX) {
        strncpy(history[history_count], line, 255);
        history[history_count][255] = '\0';
        history_count++;
    } else {
        memmove(history[0], history[1], (HISTORY_MAX - 1) * 256);
        strncpy(history[HISTORY_MAX - 1], line, 255);
        history[HISTORY_MAX - 1][255] = '\0';
    }
}

/*───────────────────────────────────────────────────────────────────────────╗
│ Line editing                                                               │
╚───────────────────────────────────────────────────────────────────────────*/

static struct termios orig_termios;
static int raw_mode;

static void disable_raw(void) {
    if (raw_mode) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
        raw_mode = 0;
    }
}

static int enable_raw(void) {
    if (!isatty(STDIN_FILENO)) return -1;
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag |= OPOST;  /* keep output processing for \n -> \r\n */
    raw.c_cflag |= CS8;
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    raw_mode = 1;
    return 0;
}

/* Clear current line on terminal and rewrite */
static void refresh_line(const char *prompt, const char *buf, int len, int pos) {
    /* Move to start, clear line, write prompt + buffer, position cursor */
    char seq[64];
    write(STDOUT_FILENO, "\r", 1);
    write(STDOUT_FILENO, prompt, strlen(prompt));
    write(STDOUT_FILENO, buf, len);
    /* Clear to end of line */
    write(STDOUT_FILENO, "\033[0K", 4);
    /* Move cursor to correct position */
    int col = strlen(prompt) + pos;
    snprintf(seq, sizeof(seq), "\r\033[%dC", col);
    write(STDOUT_FILENO, seq, strlen(seq));
}

/* Read a line with history support. Returns -1 on EOF, 0 on success. */
static int read_line(const char *prompt, char *buf, int bufsize) {
    if (enable_raw() < 0) {
        /* Not a terminal, fall back to fgets */
        write(STDOUT_FILENO, prompt, strlen(prompt));
        if (!fgets(buf, bufsize, stdin)) return -1;
        buf[strcspn(buf, "\n")] = '\0';
        return 0;
    }

    int len = 0, pos = 0;
    int hist_idx = history_count;  /* points past end = current input */
    char saved[256] = "";          /* saved current input when browsing */

    buf[0] = '\0';
    refresh_line(prompt, buf, len, pos);

    for (;;) {
        char c;
        if (read(STDIN_FILENO, &c, 1) != 1) {
            disable_raw();
            return -1;
        }

        if (c == '\r' || c == '\n') {
            write(STDOUT_FILENO, "\n", 1);
            buf[len] = '\0';
            disable_raw();
            return 0;
        }

        if (c == 4 && len == 0) {  /* Ctrl-D on empty line */
            disable_raw();
            return -1;
        }

        if (c == 127 || c == 8) {  /* Backspace */
            if (pos > 0) {
                memmove(buf + pos - 1, buf + pos, len - pos);
                pos--;
                len--;
                buf[len] = '\0';
                refresh_line(prompt, buf, len, pos);
            }
            continue;
        }

        if (c == 1) {  /* Ctrl-A: beginning of line */
            pos = 0;
            refresh_line(prompt, buf, len, pos);
            continue;
        }

        if (c == 5) {  /* Ctrl-E: end of line */
            pos = len;
            refresh_line(prompt, buf, len, pos);
            continue;
        }

        if (c == 21) {  /* Ctrl-U: kill line */
            len = pos = 0;
            buf[0] = '\0';
            refresh_line(prompt, buf, len, pos);
            continue;
        }

        if (c == 27) {  /* Escape sequence */
            char seq[2];
            if (read(STDIN_FILENO, &seq[0], 1) != 1) continue;
            if (read(STDIN_FILENO, &seq[1], 1) != 1) continue;

            if (seq[0] == '[') {
                if (seq[1] == 'A') {  /* Up arrow */
                    if (hist_idx > 0) {
                        if (hist_idx == history_count) {
                            /* Save current input */
                            strncpy(saved, buf, sizeof(saved));
                            saved[sizeof(saved) - 1] = '\0';
                        }
                        hist_idx--;
                        strncpy(buf, history[hist_idx], bufsize);
                        len = pos = strlen(buf);
                        refresh_line(prompt, buf, len, pos);
                    }
                } else if (seq[1] == 'B') {  /* Down arrow */
                    if (hist_idx < history_count) {
                        hist_idx++;
                        if (hist_idx == history_count) {
                            strncpy(buf, saved, bufsize);
                        } else {
                            strncpy(buf, history[hist_idx], bufsize);
                        }
                        len = pos = strlen(buf);
                        refresh_line(prompt, buf, len, pos);
                    }
                } else if (seq[1] == 'C') {  /* Right arrow */
                    if (pos < len) {
                        pos++;
                        refresh_line(prompt, buf, len, pos);
                    }
                } else if (seq[1] == 'D') {  /* Left arrow */
                    if (pos > 0) {
                        pos--;
                        refresh_line(prompt, buf, len, pos);
                    }
                }
            }
            continue;
        }

        /* Regular character */
        if (c >= 32 && len < bufsize - 1) {
            memmove(buf + pos + 1, buf + pos, len - pos);
            buf[pos] = c;
            pos++;
            len++;
            buf[len] = '\0';
            refresh_line(prompt, buf, len, pos);
        }
    }
}

/*───────────────────────────────────────────────────────────────────────────╗
│ Path handling                                                              │
╚───────────────────────────────────────────────────────────────────────────*/

static void normalize_path(char *path) {
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] == '/')
        path[--len] = '\0';
}

static void apply_cd(const char *target) {
    if (!target || strcmp(target, "/") == 0) {
        strcpy(cwd, "zip");
        return;
    }

    char tmp[4096];

    if (target[0] == '/') {
        snprintf(tmp, sizeof(tmp), "zip%s", target);
    } else if (strcmp(target, "..") == 0) {
        strncpy(tmp, cwd, sizeof(tmp));
        tmp[sizeof(tmp) - 1] = '\0';
        char *slash = strrchr(tmp, '/');
        if (slash && slash != tmp) {
            *slash = '\0';
            if (strlen(tmp) < 3 || strncmp(tmp, "zip", 3) != 0)
                strcpy(tmp, "zip");
        } else {
            strcpy(tmp, "zip");
        }
    } else {
        snprintf(tmp, sizeof(tmp), "%s/%s", cwd, target);
    }

    normalize_path(tmp);
    strncpy(cwd, tmp, sizeof(cwd));
    cwd[sizeof(cwd) - 1] = '\0';
}

static const char *display_path(void) {
    if (strcmp(cwd, "zip") == 0) return "/";
    if (strncmp(cwd, "zip/", 4) == 0) return cwd + 3;
    return cwd;
}

/*───────────────────────────────────────────────────────────────────────────╗
│ Argument parsing                                                           │
╚───────────────────────────────────────────────────────────────────────────*/

static int parse_args(char *line, char **argv, int max_args) {
    int argc = 0;
    char *p = line;
    while (*p && argc < max_args - 1) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        argv[argc++] = p;
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    argv[argc] = NULL;
    return argc;
}

/*───────────────────────────────────────────────────────────────────────────╗
│ Main                                                                       │
╚───────────────────────────────────────────────────────────────────────────*/

int main(void) {
    char ver[256];
    if (portator_version(ver, sizeof(ver)) > 0)
        printf("%s\n", ver);

    char line[256];
    char prompt[256];
    for (;;) {
        snprintf(prompt, sizeof(prompt), "%s> ", display_path());
        if (read_line(prompt, line, sizeof(line)) < 0)
            break;
        if (!line[0])
            continue;

        history_add(line);

        /* Parse a copy since parse_args modifies the string */
        char linecopy[256];
        strncpy(linecopy, line, sizeof(linecopy));
        linecopy[sizeof(linecopy) - 1] = '\0';

        char *argv[32];
        int argc = parse_args(linecopy, argv, 32);
        if (argc == 0) continue;

        if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0)
            break;

        if (strcmp(argv[0], "help") == 0) {
            printf("Commands:\n");
            printf("  ls [path]   - list directory contents\n");
            printf("  cd <path>   - change directory\n");
            printf("  cd ..       - go up one level\n");
            printf("  pwd         - print current directory\n");
            printf("  <app>       - run a guest app\n");
            printf("  exit        - leave the shell\n");
            continue;
        }

        if (strcmp(argv[0], "pwd") == 0) {
            printf("%s\n", display_path());
            continue;
        }

        if (strcmp(argv[0], "cd") == 0) {
            apply_cd(argc > 1 ? argv[1] : NULL);
            continue;
        }

        /* Launch guest app with PWD set */
        char pwd_env[4096];
        snprintf(pwd_env, sizeof(pwd_env), "PWD=%s", cwd);
        char *envp[] = { pwd_env, NULL };
        char **launch_argv = (argc > 1) ? argv + 1 : NULL;

        long rc = portator_launch_ex(argv[0], launch_argv, envp);
        if (rc < 0)
            printf("shell: not found: %s\n", argv[0]);
        else if (rc != 0)
            printf("shell: %s exited with status %ld\n", argv[0], rc);
    }
    return 0;
}
