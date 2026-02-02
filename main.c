/*─────────────────────────────────────────────────────────────────────────────╗
│ Portator — In-Process Emulated App Platform                                  │
│ A modified fork of Blink (ISC license) by Justine Tunney                     │
╚─────────────────────────────────────────────────────────────────────────────*/
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>

#include "blink/assert.h"
#include "blink/bus.h"
#include "blink/flag.h"
#include "blink/jit.h"
#include "blink/loader.h"
#include "blink/log.h"
#include "blink/machine.h"
#include "blink/map.h"
#include "blink/overlays.h"
#include "blink/signal.h"
#include "blink/syscall.h"
#include "blink/util.h"
#include "blink/vfs.h"
#include "blink/web.h"
#include "blink/xlat.h"

extern char **environ;
static char g_pathbuf[PATH_MAX];

static void Print(int fd, const char *s);

/*─────────────────────────────────────────────────────────────────────────────╗
│ portator new — project scaffolding                                          │
╚─────────────────────────────────────────────────────────────────────────────*/

static const char kPortatorH[] =
"/*---------------------------------------------------------------------*\\\n"
"| libportator -- Guest-side Portator API                                |\n"
"\\*---------------------------------------------------------------------*/\n"
"#ifndef PORTATOR_H_\n"
"#define PORTATOR_H_\n"
"\n"
"#include <stddef.h>\n"
"#include <stdint.h>\n"
"\n"
"/* Portator custom syscall numbers */\n"
"#define PORTATOR_SYS_PRESENT  0x7000\n"
"#define PORTATOR_SYS_POLL     0x7001\n"
"#define PORTATOR_SYS_EXIT     0x7002\n"
"#define PORTATOR_SYS_WS_SEND  0x7003\n"
"#define PORTATOR_SYS_WS_RECV  0x7004\n"
"#define PORTATOR_SYS_APP_TYPE 0x7005\n"
"#define PORTATOR_SYS_VERSION 0x7006\n"
"\n"
"/* App types */\n"
"#define PORTATOR_APP_CONSOLE  0\n"
"#define PORTATOR_APP_GFX      1\n"
"#define PORTATOR_APP_WEB      2\n"
"\n"
"/* Event types */\n"
"#define PORTATOR_KEY_DOWN     1\n"
"#define PORTATOR_KEY_UP       2\n"
"#define PORTATOR_MOUSE_DOWN   3\n"
"#define PORTATOR_MOUSE_UP     4\n"
"#define PORTATOR_MOUSE_MOVE   5\n"
"\n"
"struct PortatorEvent {\n"
"    uint32_t type;\n"
"    int32_t  x, y;\n"
"    int32_t  key;\n"
"    int32_t  button;\n"
"};\n"
"\n"
"/* Syscall wrappers -- not yet implemented on the host side.\n"
"   Console apps don't need these; they just use stdio. */\n"
"\n"
"static inline long portator_syscall(long nr, long a1, long a2, long a3) {\n"
"    long ret;\n"
"    __asm__ volatile(\"syscall\"\n"
"                     : \"=a\"(ret)\n"
"                     : \"a\"(nr), \"D\"(a1), \"S\"(a2), \"d\"(a3)\n"
"                     : \"rcx\", \"r11\", \"memory\");\n"
"    return ret;\n"
"}\n"
"\n"
"static inline long portator_exit(int code) {\n"
"    return portator_syscall(PORTATOR_SYS_EXIT, code, 0, 0);\n"
"}\n"
"\n"
"static inline long portator_app_type(void) {\n"
"    return portator_syscall(PORTATOR_SYS_APP_TYPE, 0, 0, 0);\n"
"}\n"
"\n"
"/* Write Portator version string into buf (up to len bytes).\n"
"   Returns number of bytes written, or -1 on error. */\n"
"static inline long portator_version(char *buf, long len) {\n"
"    return portator_syscall(PORTATOR_SYS_VERSION, (long)buf, len, 0);\n"
"}\n"
"\n"
"#endif /* PORTATOR_H_ */\n";

static int WriteFile(const char *path, const char *content, size_t len) {
  FILE *f = fopen(path, "w");
  if (!f) {
    Print(2, "portator: cannot create ");
    Print(2, path);
    Print(2, "\n");
    return -1;
  }
  fwrite(content, 1, len, f);
  fclose(f);
  return 0;
}

static int MakeDir(const char *path) {
  if (mkdir(path, 0755) && errno != EEXIST) {
    Print(2, "portator: cannot create directory ");
    Print(2, path);
    Print(2, "\n");
    return -1;
  }
  return 0;
}

static int CmdNew(int argc, char **argv) {
  char path[PATH_MAX];
  char source[4096];
  const char *type, *name;
  int n;

  if (argc < 4) {
    Print(2, "Usage: portator new <type> <name>\n");
    Print(2, "Types: console, gui, web\n");
    return 1;
  }
  type = argv[2];
  name = argv[3];

  if (strcmp(type, "console") != 0 &&
      strcmp(type, "gui") != 0 &&
      strcmp(type, "web") != 0) {
    Print(2, "portator: unknown type: ");
    Print(2, type);
    Print(2, "\nTypes: console, gui, web\n");
    return 1;
  }

  /* Create directories */
  if (MakeDir(name)) return 1;
  snprintf(path, sizeof(path), "%s/bin", name);
  if (MakeDir(path)) return 1;

  /* Write portator.h */
  snprintf(path, sizeof(path), "%s/portator.h", name);
  if (WriteFile(path, kPortatorH, strlen(kPortatorH))) return 1;

  /* Generate source file */
  if (strcmp(type, "console") == 0) {
    n = snprintf(source, sizeof(source),
      "#include <stdio.h>\n"
      "#include \"portator.h\"\n"
      "\n"
      "int main(void) {\n"
      "    char ver[64];\n"
      "    if (portator_version(ver, sizeof(ver)) > 0)\n"
      "        printf(\"Running on %%s\\n\", ver);\n"
      "    printf(\"Hello from %s!\\n\");\n"
      "    return 0;\n"
      "}\n", name);
  } else if (strcmp(type, "gui") == 0) {
    n = snprintf(source, sizeof(source),
      "#include <string.h>\n"
      "#include \"portator.h\"\n"
      "\n"
      "int main(void) {\n"
      "    /* TODO: graphical app */\n"
      "    return 0;\n"
      "}\n");
  } else {
    n = snprintf(source, sizeof(source),
      "#include <stdio.h>\n"
      "#include \"portator.h\"\n"
      "\n"
      "int main(void) {\n"
      "    /* TODO: web app */\n"
      "    return 0;\n"
      "}\n");
  }

  snprintf(path, sizeof(path), "%s/%s.c", name, name);
  if (WriteFile(path, source, n)) return 1;

  Print(1, "Created ");
  Print(1, name);
  Print(1, "/\n");
  Print(1, "  ");
  snprintf(path, sizeof(path), "%s/%s.c\n", name, name);
  Print(1, path);
  Print(1, "  ");
  snprintf(path, sizeof(path), "%s/portator.h\n", name);
  Print(1, path);
  Print(1, "  ");
  snprintf(path, sizeof(path), "%s/bin/\n", name);
  Print(1, path);
  Print(1, "\nBuild with:\n");
  snprintf(path, sizeof(path),
    "  portator build %s\n", name);
  Print(1, path);
  Print(1, "Run with:\n");
  snprintf(path, sizeof(path),
    "  portator run %s\n", name);
  Print(1, path);

  return 0;
}

/*─────────────────────────────────────────────────────────────────────────────╗
│ portator build — compile a guest project                                    │
╚─────────────────────────────────────────────────────────────────────────────*/

static int CmdBuild(int argc, char **argv) {
  char src[PATH_MAX];
  char out[PATH_MAX];
  const char *name;
  pid_t pid;
  int status;

  if (argc < 3) {
    Print(2, "Usage: portator build <name>\n");
    return 1;
  }
  name = argv[2];

  snprintf(src, sizeof(src), "%s/%s.c", name, name);
  snprintf(out, sizeof(out), "%s/bin/%s", name, name);

  /* Check source exists */
  if (access(src, F_OK)) {
    Print(2, "portator: source not found: ");
    Print(2, src);
    Print(2, "\n");
    return 1;
  }

  /* Ensure bin/ directory exists */
  {
    char bindir[PATH_MAX];
    snprintf(bindir, sizeof(bindir), "%s/bin", name);
    if (MakeDir(bindir)) return 1;
  }

  Print(1, "Building ");
  Print(1, name);
  Print(1, "...\n");

  pid = fork();
  if (pid < 0) {
    Print(2, "portator: fork failed\n");
    return 1;
  }
  if (pid == 0) {
    execlp("cosmocc", "cosmocc",
           "-static", "-fno-pie", "-no-pie",
           "-o", out, src, (char *)NULL);
    _exit(127);
  }
  if (waitpid(pid, &status, 0) < 0) {
    Print(2, "portator: waitpid failed\n");
    return 1;
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    Print(2, "portator: build failed\n");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
  }

  Print(1, "Built ");
  Print(1, out);
  Print(1, "\n");
  return 0;
}

/*─────────────────────────────────────────────────────────────────────────────╗
│ portator run — run a guest by project name                                  │
│ TODO: implement proper star/bin/star discovery instead of name/bin/name     │
╚─────────────────────────────────────────────────────────────────────────────*/

static int CmdRun(int argc, char **argv);

static void OnSigSys(int sig) {
  // do nothing
}

static void OnFatalSystemSignal(int sig, siginfo_t *si, void *ptr) {
  struct Machine *m = g_machine;
#ifdef __APPLE__
  sig = FixXnuSignal(m, sig, si);
#elif defined(__powerpc__) && CAN_64BIT
  sig = FixPpcSignal(m, sig, si);
#endif
#ifndef DISABLE_JIT
  if (IsSelfModifyingCodeSegfault(m, si)) return;
#endif
  g_siginfo = *si;
  unassert(m);
  unassert(m->canhalt);
  siglongjmp(m->onhalt, kMachineFatalSystemSignal);
}

static void HandleSigs(void) {
  struct sigaction sa;
  signal(SIGPIPE, SIG_IGN);
  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;
#ifdef HAVE_THREADS
  sa.sa_handler = OnSigSys;
  unassert(!sigaction(SIGSYS, &sa, 0));
#endif
  sa.sa_sigaction = OnSignal;
  unassert(!sigaction(SIGINT, &sa, 0));
  unassert(!sigaction(SIGQUIT, &sa, 0));
  unassert(!sigaction(SIGHUP, &sa, 0));
  unassert(!sigaction(SIGTERM, &sa, 0));
  unassert(!sigaction(SIGXCPU, &sa, 0));
  unassert(!sigaction(SIGXFSZ, &sa, 0));
#if !defined(__SANITIZE_THREAD__) && !defined(__SANITIZE_ADDRESS__) && \
    !defined(__FILC__)
  sa.sa_sigaction = OnFatalSystemSignal;
  sa.sa_flags = SA_SIGINFO;
  unassert(!sigaction(SIGBUS, &sa, 0));
  unassert(!sigaction(SIGILL, &sa, 0));
  unassert(!sigaction(SIGTRAP, &sa, 0));
  unassert(!sigaction(SIGSEGV, &sa, 0));
#endif
}

void TerminateSignal(struct Machine *m, int sig, int code) {
  int syssig;
  struct sigaction sa;
  KillOtherThreads(m->system);
#ifdef HAVE_JIT
  DisableJit(&m->system->jit);
#endif
  if ((syssig = XlatSignal(sig)) == -1) syssig = SIGKILL;
  FreeMachine(m);
#ifdef HAVE_JIT
  ShutdownJit();
#endif
  sa.sa_flags = 0;
  sa.sa_handler = SIG_DFL;
  sigemptyset(&sa.sa_mask);
  if (syssig != SIGKILL && syssig != SIGSTOP) {
    sigaction(syssig, &sa, 0);
  }
  kill(getpid(), syssig);
  _exit(128 + syssig);
}

/*─────────────────────────────────────────────────────────────────────────────╗
│ Portator custom syscall handler                                             │
╚─────────────────────────────────────────────────────────────────────────────*/

#define PORTATOR_VERSION "0.1.0"

extern i64 (*OnPortatorSyscall)(struct Machine *, u64, u64, u64, u64,
                                u64, u64, u64);

static i64 HandlePortatorSyscall(struct Machine *m, u64 ax, u64 di, u64 si,
                                 u64 dx, u64 r0, u64 r8, u64 r9) {
  switch (ax) {
    case 0x7006: {  /* version: di=buf_ptr, si=buf_len */
      const char *ver = "Portator " PORTATOR_VERSION;
      size_t vlen = strlen(ver);
      if ((u64)vlen >= si) vlen = si - 1;
      if (CopyToUserWrite(m, di, (void *)ver, vlen + 1))
        return -1;
      return vlen;
    }
    default:
      return -1;
  }
}

static int Exec(char *execfn, char *prog, char **argv, char **envp) {
  int i;
  struct Machine *m;
  unassert((g_machine = m = NewMachine(NewSystem(XED_MACHINE_MODE_LONG), 0)));
  m->system->exec = Exec;
  LoadProgram(m, execfn, prog, argv, envp, NULL);
  SetupCod(m);
  for (i = 0; i < 10; ++i) {
    AddStdFd(&m->system->fds, i);
  }
  struct rlimit rlim;
  if (!getrlimit(RLIMIT_NOFILE, &rlim)) {
    XlatRlimitToLinux(m->system->rlim + RLIMIT_NOFILE_LINUX, &rlim);
  }
  Blink(m);
}

static void Print(int fd, const char *s) {
  (void)!write(fd, s, strlen(s));
}

static int CmdRun(int argc, char **argv) {
  char elfpath[PATH_MAX];
  const char *name;

  if (argc < 3) {
    Print(2, "Usage: portator run <name> [args...]\n");
    return 1;
  }
  name = argv[2];

  /* TODO: implement proper program discovery (scan all star/bin/ dirs) */
  snprintf(elfpath, sizeof(elfpath), "%s/bin/%s", name, name);

  if (access(elfpath, F_OK)) {
    Print(2, "portator: program not found: ");
    Print(2, elfpath);
    Print(2, "\n");
    Print(2, "Try: portator build ");
    Print(2, name);
    Print(2, "\n");
    return 127;
  }

  /* Rewrite argv so the guest sees: <name> [args...] */
  argv[2] = elfpath;
  return Exec(elfpath, elfpath, argv + 2, environ);
}

int main(int argc, char *argv[]) {
  OnPortatorSyscall = HandlePortatorSyscall;
  SetupWeb();
  GetStartDir();
  FLAG_nolinear = !CanHaveLinearMemory();
#ifndef DISABLE_OVERLAYS
  FLAG_overlays = ":o";
#endif
  g_blink_path = argc > 0 ? argv[0] : 0;
  WriteErrorInit();
  InitMap();
  if (argc < 2) {
    Print(2, "Usage: portator PROG [ARGS...]\n");
    return 48;
  }
  if (strcmp(argv[1], "new") == 0) {
    return CmdNew(argc, argv);
  }
  if (strcmp(argv[1], "build") == 0) {
    return CmdBuild(argc, argv);
  }
#ifndef DISABLE_OVERLAYS
  if (SetOverlays(FLAG_overlays, true)) {
    Print(2, "portator: bad overlays spec\n");
    return 1;
  }
#endif
#ifndef DISABLE_VFS
  if (VfsInit(FLAG_prefix)) {
    Print(2, "portator: vfs init failed\n");
    return 1;
  }
#endif
  HandleSigs();
  InitBus();
  if (strcmp(argv[1], "run") == 0) {
    return CmdRun(argc, argv);
  }
  if (!Commandv(argv[1], g_pathbuf, sizeof(g_pathbuf))) {
    Print(2, "portator: command not found: ");
    Print(2, argv[1]);
    Print(2, "\n");
    return 127;
  }
  argv[1] = g_pathbuf;
  return Exec(g_pathbuf, g_pathbuf, argv + 1, environ);
}
