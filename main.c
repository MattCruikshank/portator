/*─────────────────────────────────────────────────────────────────────────────╗
│ Portator — Actually Portable Executable ELF Emulator with embedded apps      │
│ Portator (ISC license) by Matt Cruikshank                                    │
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
#include <utime.h>
#include <libgen.h>
#include <dirent.h>

#include "config.h"
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
#include "web_server.h"

extern char **environ;
static char g_pathbuf[PATH_MAX];

static void Print(int fd, const char *s);

/*─────────────────────────────────────────────────────────────────────────────╗
│ portator new — project scaffolding                                          │
╚─────────────────────────────────────────────────────────────────────────────*/

static int ExtractFromZip(const char *relpath) {
  char zippath[PATH_MAX];
  char outpath[PATH_MAX];
  char parentdir[PATH_MAX];
  char *parent;
  FILE *fin, *fout;
  char buf[4096];
  size_t n;

  snprintf(zippath, sizeof(zippath), "/zip/%s", relpath);
  snprintf(outpath, sizeof(outpath), "./%s", relpath);

  /* Create parent directories recursively */
  snprintf(parentdir, sizeof(parentdir), "%s", outpath);
  parent = dirname(parentdir);
  {
    char tmp[PATH_MAX];
    char *p;
    snprintf(tmp, sizeof(tmp), "%s", parent);
    for (p = tmp + 1; *p; p++) {
      if (*p == '/') {
        *p = '\0';
        mkdir(tmp, 0755);
        *p = '/';
      }
    }
    if (mkdir(tmp, 0755) && errno != EEXIST) {
      Print(2, "portator: cannot create directory ");
      Print(2, tmp);
      Print(2, "\n");
      return -1;
    }
  }

  fin = fopen(zippath, "r");
  if (!fin) {
    Print(2, "portator: not found in zip: ");
    Print(2, zippath);
    Print(2, "\n");
    return -1;
  }
  fout = fopen(outpath, "w");
  if (!fout) {
    fclose(fin);
    Print(2, "portator: cannot create ");
    Print(2, outpath);
    Print(2, "\n");
    return -1;
  }
  while ((n = fread(buf, 1, sizeof(buf), fin)) > 0) {
    fwrite(buf, 1, n, fout);
  }
  fclose(fin);
  fclose(fout);
  utime(outpath, NULL);
  return 0;
}

// TODO: Port to an app, make it dynamic
static const char *kSharedFiles[] = {
  "include/portator.h",
  "include/cjson/cJSON.h",
  "include/mustach.h",
  "include/mustach-wrap.h",
  "include/mustach-cjson.h",
  "src/cJSON.c",
  "src/mustach.c",
  "src/mustach-wrap.c",
  "src/mustach-cjson.c",
  NULL
};

// TODO: Port to an app
static int ExtractSharedFiles(void) {
  int i;
  for (i = 0; kSharedFiles[i]; i++) {
    if (ExtractFromZip(kSharedFiles[i])) return -1;
    Print(1, "  ");
    Print(1, kSharedFiles[i]);
    Print(1, "\n");
  }
  return 0;
}

static int CmdInit(int argc, char **argv) {
  (void)argc;
  (void)argv;
  Print(1, "Extracting shared files...\n");
  if (ExtractSharedFiles()) return 1;
  Print(1, "Done.\n");
  return 0;
}

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

  /* Extract shared files if needed */
  if (access("include", F_OK) || access("src", F_OK)) {
    Print(1, "Extracting shared files...\n");
    if (ExtractSharedFiles()) return 1;
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
           "-I./include", "-I./include/cjson",
           "-DNO_OPEN_MEMSTREAM",
           "-o", out, src,
           "./src/cJSON.c",
           "./src/mustach.c",
           "./src/mustach-wrap.c",
           "./src/mustach-cjson.c",
           (char *)NULL);
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

/* Build a JSON string like {"apps":[{"name":"snake"},{"name":"list"}]}
   by scanning for directories where <name>/<name>.c exists.
   Caller must free() the result. */
static char *BuildAppListJson(void) {
  DIR *d;
  struct dirent *ent;
  char probe[PATH_MAX];
  char *json;
  size_t cap = 512, len = 0;
  int first = 1;

  json = (char *)malloc(cap);
  if (!json) return NULL;

  len += snprintf(json + len, cap - len, "{\"apps\":[");

  /* Scan local ./<name>/bin/<name> and /zip/apps/<name>/bin/<name> */
  const char *dirs[] = { ".", "/zip/apps", NULL };
  int di;
  for (di = 0; dirs[di]; di++) {
    d = opendir(dirs[di]);
    if (!d) continue;
    while ((ent = readdir(d)) != NULL) {
      if (ent->d_name[0] == '.') continue;
      snprintf(probe, sizeof(probe), "%s/%s/bin/%s",
               dirs[di], ent->d_name, ent->d_name);
      if (access(probe, F_OK) != 0) continue;
      /* skip if already listed (local overrides bundled) */
      char needle[PATH_MAX];
      snprintf(needle, sizeof(needle), "\"%s\"", ent->d_name);
      if (strstr(json, needle)) continue;
      /* grow buffer if needed */
      size_t need = len + strlen(ent->d_name) + 32;
      if (need > cap) {
        cap = need * 2;
        char *tmp = (char *)realloc(json, cap);
        if (!tmp) { free(json); closedir(d); return NULL; }
        json = tmp;
      }
      if (!first) json[len++] = ',';
      len += snprintf(json + len, cap - len,
                      "{\"name\":\"%s\"}", ent->d_name);
      first = 0;
    }
    closedir(d);
  }

  /* grow if needed for closing */
  if (len + 4 > cap) {
    char *tmp = (char *)realloc(json, len + 4);
    if (!tmp) { free(json); return NULL; }
    json = tmp;
  }
  len += snprintf(json + len, 4, "]}");
  return json;
}

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
    case 0x7007: {  /* list: di=buf_ptr, si=buf_len */
      char *json = BuildAppListJson();
      if (!json) return -1;
      size_t jlen = strlen(json) + 1;
      if (!di || !si) { free(json); return jlen; }
      if (jlen > (u64)si) jlen = si;
      i64 rc = CopyToUserWrite(m, di, (void *)json, jlen) ? -1 : (i64)jlen;
      free(json);
      return rc;
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

static int CmdRunForked(int argc, char **argv) {
  char elfpath[PATH_MAX];
  char appdata[PATH_MAX];
  const char *name;
  int bundled = 0;

  if (argc < 3) {
    Print(2, "Usage: portator run <name> [args...]\n");
    return 1;
  }
  name = argv[2];

  /* Try local first, then bundled */
  snprintf(elfpath, sizeof(elfpath), "%s/bin/%s", name, name);
  LOGF("CmdRun: trying local path '%s'", elfpath);
  if (access(elfpath, F_OK)) {
    LOGF("CmdRun: local access failed (errno %d), trying bundled", errno);
    snprintf(elfpath, sizeof(elfpath), "/zip/apps/%s/bin/%s", name, name);
    LOGF("CmdRun: trying bundled path '%s'", elfpath);
    if (access(elfpath, F_OK)) {
      LOGF("CmdRun: bundled access failed (errno %d)", errno);
      Print(2, "portator: program not found: ");
      Print(2, name);
      Print(2, "\n");
      Print(2, "Try: portator build ");
      Print(2, name);
      Print(2, "\n");
      return 127;
    }
    LOGF("CmdRun: bundled access succeeded");
    bundled = 1;
  } else {
    LOGF("CmdRun: local access succeeded");
  }

  /* Mount /zip so guest can access bundled files */
#ifndef DISABLE_VFS
  VfsMountZip();
  LOGF("CmdRun: mounted /zip");

  /* Ensure /tmp exists so guest tmpfile() works (mustach needs it) */
  VfsMkdir(AT_FDCWD, "/tmp", 0755);

  /* Mount app data directory so guest can access /app/ */
  if (bundled) {
    snprintf(appdata, sizeof(appdata), "/zip/apps/%s", name);
  } else {
    /* For local apps, use <name>/zip/ if it exists */
    snprintf(appdata, sizeof(appdata), "%s/zip", name);
  }
  // LOGF("CmdRun: mounting '%s' at /app", appdata);
  // /* Create /app mount point and mount app data */
  // VfsMkdir(AT_FDCWD, "/app", 0755);
  // VfsMount(appdata, "/app", "hostfs", 0, NULL);
#endif

  LOGF("CmdRun: executing '%s' (bundled=%d)", elfpath, bundled);
  /* Rewrite argv so the guest sees: <name> [args...] */
  argv[2] = elfpath;
  return Exec(elfpath, elfpath, argv + 2, environ);
}

static int CmdRun(int argc, char **argv) {
  pid_t pid;
  int status;

  pid = fork();
  if (pid < 0) {
    Print(2, "portator: fork failed\n");
    return 1;
  }
  if (pid == 0) {
    CmdRunForked(argc, argv);
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

  return 0;
}

/*─────────────────────────────────────────────────────────────────────────────╗
│ portator web — start the web UI                                              │
╚─────────────────────────────────────────────────────────────────────────────*/

static int CmdWeb(int argc, char **argv) {
  int port = 6711;
  if (argc >= 3) port = atoi(argv[2]);
  if (port <= 0 || port > 65535) {
    Print(2, "portator: invalid port\n");
    return 1;
  }
  if (WebServerStart(port, "/zip/wwwroot")) return 1;
  Print(1, "Press Enter to stop the server...\n");
  (void)getchar();
  WebServerStop();
  return 0;
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
  if (argc < 2 || strcmp(argv[1], "help") == 0) {
    Print(1, "\n");
    Print(1, "  Portator " PORTATOR_VERSION "\n");
    Print(1, "  In-Process Emulated App Platform\n");
    Print(1, "\n");
    Print(1, "  Usage: portator <command> [args...]\n");
    Print(1, "\n");
    Print(1, "  Commands:\n");
    Print(1, "    new <type> <name>   Create a new project (console, gui, web)\n");
    Print(1, "    build <name>        Compile a project with cosmocc\n");
    Print(1, "    run <name>          Run a program in the emulator\n");
    Print(1, "    list                List discovered programs\n");
    Print(1, "    init                Extract shared include/src files\n");
    Print(1, "    web [port]          Start the web UI (default: 6711)\n");
    Print(1, "    credits             Show third-party credits\n");
    Print(1, "    license             Show license information\n");
    Print(1, "    help                Show this message\n");
    Print(1, "\n");
    Print(1, "  https://portator.net\n");
    Print(1, "\n");
    return 0;
  }
  if (strcmp(argv[1], "credits") == 0) {
    Print(1, "\n");
    Print(1, "  Portator " PORTATOR_VERSION " — Credits\n");
    Print(1, "\n");
    Print(1, "  Blink            x86-64 emulator (ISC license)\n");
    Print(1, "                   Justine Tunney\n");
    Print(1, "                   https://github.com/jart/blink\n");
    Print(1, "\n");
    Print(1, "  Cosmopolitan     portable C toolchain (ISC license)\n");
    Print(1, "                   Justine Tunney\n");
    Print(1, "                   https://github.com/jart/cosmopolitan\n");
    Print(1, "\n");
    Print(1, "  cJSON            JSON parser for C (MIT license)\n");
    Print(1, "                   Dave Gamble\n");
    Print(1, "                   https://github.com/DaveGamble/cJSON\n");
    Print(1, "\n");
    Print(1, "  mustach          Mustache templates for C (0BSD license)\n");
    Print(1, "                   Jose Bollo\n");
    Print(1, "                   https://gitlab.com/jobol/mustach\n");
    Print(1, "\n");
    Print(1, "  CivetWeb         HTTP/WebSocket server (MIT license)\n");
    Print(1, "                   CivetWeb contributors\n");
    Print(1, "                   https://github.com/civetweb/civetweb\n");
    Print(1, "\n");
    Print(1, "  Built with the assistance of:\n");
    Print(1, "\n");
    Print(1, "  Claude Code      AI coding assistant\n");
    Print(1, "                   Anthropic (Claude Opus 4.5)\n");
    Print(1, "                   https://claude.ai/claude-code\n");
    Print(1, "\n");
    return 0;
  }
  if (strcmp(argv[1], "init") == 0) {
    return CmdInit(argc, argv);
  }
  if (strcmp(argv[1], "build") == 0) {
    return CmdBuild(argc, argv);
  }
  if (strcmp(argv[1], "web") == 0) {
    return CmdWeb(argc, argv);
  }
#ifndef DISABLE_OVERLAYS
  if (SetOverlays(FLAG_overlays, true)) {
    Print(2, "portator: bad overlays spec\n");
    return 1;
  }
#endif
#ifndef DISABLE_VFS
  /* Use current directory as VFS prefix to avoid permission issues */
  {
    static char cwdbuf[PATH_MAX];
    if (getcwd(cwdbuf, sizeof(cwdbuf))) {
      FLAG_prefix = cwdbuf;
    }
  }
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
  if (strcmp(argv[1], "list") == 0) {
    char *list_argv[] = { argv[0], (char *)"run", (char *)"list", NULL };
    return CmdRun(3, list_argv);
  }
  if (strcmp(argv[1], "license") == 0) {
    char *lic_argv[] = { argv[0], (char *)"run", (char *)"license", NULL };
    return CmdRun(3, lic_argv);
  }
  if (strcmp(argv[1], "new") == 0) {
    /* portator new <type> <name> -> run new <type> <name> */
    char *new_argv[6] = { argv[0], (char *)"run", (char *)"new", NULL };
    int new_argc = 3;
    for (int i = 2; i < argc && new_argc < 5; i++)
      new_argv[new_argc++] = argv[i];
    new_argv[new_argc] = NULL;
    return CmdRun(new_argc, new_argv);
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
