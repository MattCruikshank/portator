/*─────────────────────────────────────────────────────────────────────────────╗
│ Portator — In-Process Emulated App Platform                                │
│ A modified fork of Blink (ISC license) by Justine Tunney                   │
╚─────────────────────────────────────────────────────────────────────────────*/
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
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

int main(int argc, char *argv[]) {
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
  if (!Commandv(argv[1], g_pathbuf, sizeof(g_pathbuf))) {
    Print(2, "portator: command not found: ");
    Print(2, argv[1]);
    Print(2, "\n");
    return 127;
  }
  argv[1] = g_pathbuf;
  return Exec(g_pathbuf, g_pathbuf, argv + 1, environ);
}
