/* Generated code.  Do not edit. */
/*---------------------------------------------------------------------*\
| libportator -- Guest-side Portator API                                |
\*---------------------------------------------------------------------*/
#ifndef PORTATOR_H_
#define PORTATOR_H_

#include <stddef.h>
#include <stdint.h>

/* Portator custom syscall numbers */
#define PORTATOR_SYS_PRESENT  0x7000
#define PORTATOR_SYS_POLL     0x7001
#define PORTATOR_SYS_EXIT     0x7002
#define PORTATOR_SYS_WS_SEND  0x7003
#define PORTATOR_SYS_WS_RECV  0x7004
#define PORTATOR_SYS_APP_TYPE 0x7005
#define PORTATOR_SYS_VERSION 0x7006
#define PORTATOR_SYS_LIST    0x7007

/* App types */
#define PORTATOR_APP_CONSOLE  0
#define PORTATOR_APP_GFX      1
#define PORTATOR_APP_WEB      2

/* Event types */
#define PORTATOR_KEY_DOWN     1
#define PORTATOR_KEY_UP       2
#define PORTATOR_MOUSE_DOWN   3
#define PORTATOR_MOUSE_UP     4
#define PORTATOR_MOUSE_MOVE   5

struct PortatorEvent {
    uint32_t type;
    int32_t  x, y;
    int32_t  key;
    int32_t  button;
};

/* Syscall wrappers -- not yet implemented on the host side.
   Console apps don't need these; they just use stdio. */

static inline long portator_syscall(long nr, long a1, long a2, long a3) {
    long ret;
    __asm__ volatile("syscall"
                     : "=a"(ret)
                     : "a"(nr), "D"(a1), "S"(a2), "d"(a3)
                     : "rcx", "r11", "memory");
    return ret;
}

static inline long portator_exit(int code) {
    return portator_syscall(PORTATOR_SYS_EXIT, code, 0, 0);
}

static inline long portator_app_type(void) {
    return portator_syscall(PORTATOR_SYS_APP_TYPE, 0, 0, 0);
}

/* Write Portator version string into buf (up to len bytes).
   Returns number of bytes written, or -1 on error. */
static inline long portator_version(char *buf, long len) {
    return portator_syscall(PORTATOR_SYS_VERSION, (long)buf, len, 0);
}

/* Get JSON listing of apps.
   Call with buf=NULL/len=0 to get required size (including NUL).
   Call with allocated buffer to fill it. Returns size or -1. */
static inline long portator_list(char *buf, long len) {
    return portator_syscall(PORTATOR_SYS_LIST, (long)buf, len, 0);
}

#endif /* PORTATOR_H_ */
