// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <NSCAPI.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <stdint.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/syscall.h>
// close_range() landed in 5.9 with the same number on every architecture;
// spell it out so a build against older headers still tries the fast path.
#ifdef SYS_close_range
#define NSCP_SYS_CLOSE_RANGE SYS_close_range
#else
#define NSCP_SYS_CLOSE_RANGE 436
#endif
#endif

#include <algorithm>
#include <limits>
#include <list>
#include <mutex>
#include <shared_mutex>
#include <bytes/buffer.hpp>
#include <process/execute_process.hpp>
#include <string>
#include <vector>

namespace {
// strerror() is not thread-safe and this runs on worker threads. strerror_r
// comes in the XSI (int) and GNU (char *) flavours; both are handled.
std::string describe_errno(int, const char *buf) { return buf; }
std::string describe_errno(const char *msg, const char *) { return msg ? msg : ""; }
std::string errno_text(int err) {
  char buf[256] = {0};
  return describe_errno(strerror_r(err, buf, sizeof(buf)), buf);
}
}  // namespace

#define BUFFER_SIZE 4096

// Upper bound on captured child output. A check is expected to print one Nagios
// line; without a cap a script (buggy or hostile) can emit hundreds of MB within
// its timeout window and balloon the service's memory. 8 MiB is far above any
// legitimate check output. Once reached we keep reading (so the child never
// blocks on a full pipe and the timeout stays enforceable) but discard the rest.
#define MAX_OUTPUT_BYTES (8u * 1024u * 1024u)

namespace {
// The truncation marker and the content ceiling that leaves room for it, so the
// captured string is a strict <= MAX_OUTPUT_BYTES bound (marker included).
const char kOutputTruncMarker[] = "\n[output truncated]";
const std::size_t kOutputContentCap = MAX_OUTPUT_BYTES - (sizeof(kOutputTruncMarker) - 1);
}  // namespace

typedef hlp::buffer<char> buffer_type;

namespace {
// Close every descriptor above stderr in the child, between fork() and exec():
// only async-signal-safe calls, no allocation. Nothing above stderr is the
// script's business - listener sockets, the log file, other scripts' pipes
// and whatever else the service holds without close-on-exec would otherwise
// be the child's to read and write. On Linux close_range() does it in one
// call (ENOSYS before 5.9); failing that /proc/self/fd names exactly the
// descriptors that are open, read with raw getdents64 into a stack buffer
// (what CPython's subprocess does). The bounded sweep is the last resort
// only, for a system with neither: it costs one close() per possible
// descriptor and stops at `max_fd`.
#if defined(__linux__)
struct dirent64_compat {
  uint64_t d_ino;
  int64_t d_off;
  unsigned short d_reclen;
  unsigned char d_type;
  char d_name[1];
};
bool close_from_proc() {
  const int dir = open("/proc/self/fd", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (dir < 0) return false;
  alignas(8) char buf[4096];
  bool ok = true;
  for (;;) {
    const long n = syscall(SYS_getdents64, dir, buf, sizeof(buf));
    if (n == 0) break;
    if (n < 0) {
      ok = false;
      break;
    }
    for (long off = 0; off < n;) {
      const dirent64_compat *d = reinterpret_cast<const dirent64_compat *>(buf + off);
      off += d->d_reclen;
      int fd = 0;
      bool numeric = d->d_name[0] != '\0';
      for (const char *p = d->d_name; *p != '\0'; ++p) {
        if (*p < '0' || *p > '9' || fd > 100000000) {
          numeric = false;
          break;
        }
        fd = fd * 10 + (*p - '0');
      }
      if (numeric && fd > STDERR_FILENO && fd != dir) close(fd);
    }
  }
  close(dir);
  return ok;
}
#endif
void close_above_stdio(long max_fd) {
#if defined(__linux__)
  if (syscall(NSCP_SYS_CLOSE_RANGE, 3, ~0U, 0) == 0) return;
  if (close_from_proc()) return;
#endif
  for (long fd = 3; fd < max_fd; ++fd) close(static_cast<int>(fd));
}
}  // namespace

namespace {
// The children this launcher has forked and not yet reaped, so that kill_all()
// (module unload) can end them. `group` records that `kill tree` made the child
// the leader of a session - and so of a process group - of its own, in which
// case a signal to -pid reaches everything the script started as well.
// `unloading` records that kill_all() is what ended it, so the check can say so.
//
// The registry must never hold a pid the kernel may have handed to another
// process, and must never miss a live child:
//  - fork_gate is held shared from just before fork() until the child is
//    registered, and kill_all() takes it exclusively, so kill_all() cannot run
//    between a fork and its registration. Shared, so that script launches do
//    not queue behind one another's fork() (a page-table copy of the whole
//    agent); only an unload waits for the launches in flight;
//  - an entry is removed before its pid is reaped: the parent waits for the
//    child with WNOWAIT, which leaves it a zombie whose pid cannot be reused,
//    takes it out of the registry, and only then calls waitpid().
// children_mutex guards the list itself and is only ever held briefly. Lock
// order: fork_gate before children_mutex.
struct running_child {
  pid_t pid;
  bool group;
  bool unloading;
};
std::shared_mutex fork_gate;
std::mutex children_mutex;
std::list<running_child> children;

// Deliver `sig` to a child - to its whole process group when it leads one.
// kill(-pid) fails with ESRCH if the child has not reached its setsid() yet;
// the direct kill then ends it before it has started anything.
void signal_child(pid_t pid, bool group, int sig) {
  if (group && kill(-pid, sig) == 0) return;
  kill(pid, sig);
}

// Take `pid` out of the registry. Returns whether kill_all() signalled it
// (finish_child() decides whether that is what ended it). Idempotent, so the scope guard below can call it again on every path.
bool unregister_child(pid_t pid) {
  const std::lock_guard<std::mutex> lock(children_mutex);
  bool unloading = false;
  for (auto it = children.begin(); it != children.end();) {
    if (it->pid == pid) {
      unloading = unloading || it->unloading;
      it = children.erase(it);
    } else {
      ++it;
    }
  }
  return unloading;
}

// Backstop for the return paths that never get as far as reaping (a failed
// wait): whatever happens, the entry does not outlive this call.
class child_registration_guard {
  const pid_t pid_;

 public:
  explicit child_registration_guard(pid_t pid) : pid_(pid) {}
  ~child_registration_guard() { unregister_child(pid_); }
  child_registration_guard(const child_registration_guard&) = delete;
  child_registration_guard& operator=(const child_registration_guard&) = delete;
};

enum class wait_result { exited, running, failed };

// Wait for the child to exit without reaping it (WNOWAIT): it stays a zombie,
// so its pid stays reserved until reap_child() below.
wait_result wait_exited(pid_t pid, bool block) {
  for (;;) {
    siginfo_t info;
    memset(&info, 0, sizeof(info));
    const int r = waitid(P_PID, static_cast<id_t>(pid), &info, WEXITED | WNOWAIT | (block ? 0 : WNOHANG));
    if (r < 0) {
      if (errno == EINTR) continue;
      return wait_result::failed;
    }
    return info.si_pid == pid ? wait_result::exited : wait_result::running;
  }
}

// Unregister, then reap. Returns whether kill_all() signalled the child.
bool reap_child(pid_t pid, int& status) {
  const bool unloading = unregister_child(pid);
  status = 0;
  while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
  }
  return unloading;
}

// "SIGKILL" for 9. glibc 2.32 and later know every signal of the platform;
// elsewhere the table covers the POSIX ones, and anything else (a realtime
// signal) is left to the number.
std::string signal_name(int sig) {
#if defined(__GLIBC__) && (__GLIBC__ > 2 || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 32))
  if (const char* abbrev = sigabbrev_np(sig)) return std::string("SIG") + abbrev;
#endif
  static const struct {
    int sig;
    const char* name;
  } names[] = {{SIGHUP, "SIGHUP"},   {SIGINT, "SIGINT"},       {SIGQUIT, "SIGQUIT"},   {SIGILL, "SIGILL"},   {SIGTRAP, "SIGTRAP"},
               {SIGABRT, "SIGABRT"}, {SIGBUS, "SIGBUS"},       {SIGFPE, "SIGFPE"},     {SIGKILL, "SIGKILL"}, {SIGUSR1, "SIGUSR1"},
               {SIGSEGV, "SIGSEGV"}, {SIGUSR2, "SIGUSR2"},     {SIGPIPE, "SIGPIPE"},   {SIGALRM, "SIGALRM"}, {SIGTERM, "SIGTERM"},
               {SIGCHLD, "SIGCHLD"}, {SIGCONT, "SIGCONT"},     {SIGSTOP, "SIGSTOP"},   {SIGTSTP, "SIGTSTP"}, {SIGTTIN, "SIGTTIN"},
               {SIGTTOU, "SIGTTOU"}, {SIGURG, "SIGURG"},       {SIGXCPU, "SIGXCPU"},   {SIGXFSZ, "SIGXFSZ"}, {SIGVTALRM, "SIGVTALRM"},
               {SIGPROF, "SIGPROF"}, {SIGSYS, "SIGSYS"}};
  for (const auto& n : names) {
    if (n.sig == sig) return n.name;
  }
  return std::string();
}

// End a child that is still (or may still be) running: SIGTERM, up to two
// seconds for it to exit, then SIGKILL, then wait for it - without reaping it.
// The SIGKILL goes out whatever the SIGTERM achieved. The leader exiting says
// nothing about the rest of its group (a helper that traps SIGTERM outlives a
// shell that does not), and a wait that failed says nothing about either.
// Signalling after the leader has exited is safe because it has not been
// reaped yet: an unreaped zombie keeps its pid, and with it the group id,
// reserved. An agent started with SIGCHLD ignored has its children reaped by
// the kernel, which voids that guarantee - as it did for every launcher before
// this one - and the kill is still the safer side to err on.
void terminate_child(pid_t pid, bool group) {
  signal_child(pid, group, SIGTERM);
  for (int i = 0; i < 20; ++i) {
    if (wait_exited(pid, false) != wait_result::running) break;
    struct timespec ts;
    ts.tv_sec = 0;
    ts.tv_nsec = 100 * 1000 * 1000;  // 100ms
    nanosleep(&ts, nullptr);
  }
  signal_child(pid, group, SIGKILL);
  wait_exited(pid, true);
}
}  // namespace

void process::kill_all() {
  // Called on module unload, with worker threads possibly still blocked in
  // drain_with_timeout on a script that will not finish. SIGKILL, as the
  // Windows launcher's TerminateProcess: the point of unload is to stop
  // waiting. Nothing here waits; the worker that owns each child reaps it and
  // reports that the module was unloading.
  //
  // With `kill tree` the whole group is killed. Without it only the script is:
  // it has no group of its own to signal, and a helper it backgrounded keeps
  // running - and, holding the output pipe, keeps that worker waiting until
  // the script's timeout.
  //
  // A child that finished but has not been reaped yet is marked too - the
  // SIGKILL does nothing to it - which is why finish_child() only believes the
  // mark when the status says the child died of SIGKILL.
  const std::unique_lock<std::shared_mutex> gate(fork_gate);
  const std::lock_guard<std::mutex> lock(children_mutex);
  for (running_child& c : children) {
    c.unloading = true;
    signal_child(c.pid, c.group, SIGKILL);
  }
}

namespace {
// Drain the pipe up to `deadline` (an absolute time). Returns the bytes read
// on success and clears `timed_out` / `had_error`. On timeout the caller is
// expected to terminate the child.
std::string drain_with_timeout(int fd, time_t deadline, bool& timed_out, bool& had_error) {
  std::string out;
  buffer_type buffer(BUFFER_SIZE);
  for (;;) {
    const time_t now = time(nullptr);
    if (now >= deadline) {
      timed_out = true;
      return out;
    }
    // poll(), not select(): FD_SET on a descriptor past FD_SETSIZE writes off
    // the end of the stack bitmap, and a busy agent (ten io threads per
    // socket server plus the web server) can hold that many descriptors.
    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    // Clamp the wait: (deadline - now) * 1000 overflows int for a large
    // configured timeout, and a negative poll timeout means "block forever",
    // which would turn this bounded drain unbounded. The loop re-arms, so a
    // capped single wait costs nothing.
    const long long remaining_ms = static_cast<long long>(deadline - now) * 1000;
    const int poll_ms = remaining_ms > static_cast<long long>((std::numeric_limits<int>::max)()) ? (std::numeric_limits<int>::max)() : static_cast<int>(remaining_ms);
    const int ready = poll(&pfd, 1, poll_ms);
    if (ready < 0) {
      if (errno == EINTR) continue;
      had_error = true;
      return out;
    }
    if (ready == 0) {
      timed_out = true;
      return out;
    }
    const ssize_t n = read(fd, buffer.get(), buffer.size() - 1);
    if (n < 0) {
      if (errno == EINTR) continue;
      had_error = true;
      return out;
    }
    if (n == 0) {
      // EOF: child closed its end of the pipe.
      return out;
    }
    // Append up to the content cap; past it keep draining but discard, so the
    // child is never blocked on a full pipe (which would defeat the timeout) yet
    // memory stays bounded. The marker fits within MAX_OUTPUT_BYTES.
    if (out.size() < kOutputContentCap) {
      const std::size_t room = kOutputContentCap - out.size();
      out.append(buffer.get(), std::min(static_cast<std::size_t>(n), room));
      if (out.size() >= kOutputContentCap) out.append(kOutputTruncMarker);
    }
  }
}

NSCAPI::nagiosReturn map_exit_status(int status) {
  if (!WIFEXITED(status)) {
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  return WEXITSTATUS(status);
}

// The result of a reaped child: its exit code, or UNKNOWN with a message that
// says why when it did not exit by itself. Whatever the script printed before
// it died is kept below the message.
// `unloading` is only believed when the child died of SIGKILL: kill_all() also
// marks a child that had already exited by itself and was waiting to be reaped.
bool killed_by_unload(bool unloading, int status) { return unloading && WIFSIGNALED(status) && WTERMSIG(status) == SIGKILL; }

NSCAPI::nagiosReturn finish_child(const process::exec_arguments& args, int status, bool unloading, std::string& output) {
  std::string reason;
  if (killed_by_unload(unloading, status)) {
    reason = "Command " + args.alias + " was killed: the module is unloading";
  } else if (WIFSIGNALED(status)) {
    const int sig = WTERMSIG(status);
    const std::string name = signal_name(sig);
    reason = "Command " + args.alias + " was terminated by signal " + std::to_string(sig) + (name.empty() ? std::string() : " (" + name + ")");
  } else {
    return map_exit_status(status);
  }
  output = output.empty() ? reason : reason + "\n" + output;
  return NSCAPI::query_return_codes::returnUNKNOWN;
}

// Run an argv vector via fork + execvp. No shell is involved, so attacker-
// controlled argv elements cannot become metacharacters.
int execute_argv(const process::exec_arguments& args, std::string& output) {
  if (args.argv.empty()) {
    output = "Refusing to execute an empty command";
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }

  // Build the execvp argv BEFORE forking. Only async-signal-safe calls are
  // legal between fork() and exec() in a multithreaded process, and this one is
  // emphatically multithreaded - each socket server runs a 10-thread io pool by
  // default, plus the scheduler pool and the collectors. Allocating in the
  // child (this vector used to be built there, and reserve() is a malloc) can
  // deadlock against an allocator lock that some other thread held at the
  // moment of the fork, and whose owner does not exist in the child. The parent
  // then blocks in drain_with_timeout until the command timeout expires and
  // reports a bogus "didn't terminate" - an intermittent, load-dependent check
  // failure that is close to undiagnosable from the logs.
  //
  // The backing std::strings in args.argv stay alive across the fork, so the
  // child only indexes an array that already exists.
  std::vector<char*> cargs;
  cargs.reserve(args.argv.size() + 1);
  for (const auto& a : args.argv) {
    cargs.push_back(const_cast<char*>(a.c_str()));
  }
  cargs.push_back(nullptr);

  // Close-on-exec from the moment the pipe exists: another worker may fork
  // between pipe() and this fork, and its child would otherwise carry this
  // script's write end - it could then write into this script's output, and
  // this script's read end would not see EOF until that unrelated child had
  // exited too. pipe2() sets the flag atomically where it exists; elsewhere
  // fcntl() closes the window as far as it can be closed.
  int pipefd[2];
#if defined(__linux__) && defined(O_CLOEXEC)
  const int piped = pipe2(pipefd, O_CLOEXEC);
#else
  int piped = pipe(pipefd);
  if (piped == 0 && (fcntl(pipefd[0], F_SETFD, FD_CLOEXEC) != 0 || fcntl(pipefd[1], F_SETFD, FD_CLOEXEC) != 0)) {
    const int saved = errno;
    close(pipefd[0]);
    close(pipefd[1]);
    errno = saved;
    piped = -1;
  }
#endif
  if (piped != 0) {
    output = "Failed to create pipe: ";
    output += errno_text(errno);
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  // A pipe end at 0, 1 or 2 - the service was started with that descriptor
  // closed, so the pipe took the lowest free one - would collide with the
  // dup2() in the child: dup2(1, 1) is a no-op that leaves close-on-exec set,
  // and the script would exec with no stdout. Move such an end above stdio.
  for (int i = 0; i < 2; ++i) {
    if (pipefd[i] > STDERR_FILENO) continue;
    const int raised = fcntl(pipefd[i], F_DUPFD_CLOEXEC, 3);
    if (raised < 0) {
      const int saved = errno;
      close(pipefd[0]);
      close(pipefd[1]);
      output = "Failed to create pipe: ";
      output += errno_text(saved);
      return NSCAPI::query_return_codes::returnUNKNOWN;
    }
    close(pipefd[i]);
    pipefd[i] = raised;
  }

  // Bound for the last-resort descriptor sweep in the child (see
  // close_above_stdio). Read here, in the parent: getrlimit and sysconf are
  // not on the async-signal-safe list. The soft nofile limit is what bounds
  // the descriptors the service can hold; an unlimited one is capped so the
  // sweep, if it is ever taken, stays at a million close() calls at most.
  const long max_fd_cap = 1L << 20;
  long max_fd = sysconf(_SC_OPEN_MAX);
  struct rlimit nofile;
  if (getrlimit(RLIMIT_NOFILE, &nofile) == 0) {
    if (nofile.rlim_cur == RLIM_INFINITY) {
      max_fd = max_fd_cap;
    } else if (static_cast<long>(nofile.rlim_cur) > max_fd) {
      max_fd = static_cast<long>(nofile.rlim_cur);
    }
  }
  if (max_fd < 1024) max_fd = 1024;
  if (max_fd > max_fd_cap) max_fd = max_fd_cap;

  // Every script reads stdin from /dev/null. Inheriting the agent's own stdin
  // let a script that reads it consume what was typed at an `nscp test`
  // prompt, and - once `kill tree` moves the script off the terminal's
  // foreground group - be stopped by SIGTTIN until its timeout. Opened here,
  // not in the child, so a failure is reported rather than hidden behind 127.
  int devnull = open("/dev/null", O_RDONLY | O_CLOEXEC);
  if (devnull >= 0 && devnull <= STDERR_FILENO) {
    // Same collision as the pipe ends above: dup2(0, 0) would keep close-on-exec.
    const int raised = fcntl(devnull, F_DUPFD_CLOEXEC, 3);
    close(devnull);
    devnull = raised;
  }
  if (devnull < 0) {
    const int saved = errno;
    close(pipefd[0]);
    close(pipefd[1]);
    output = "Failed to open /dev/null: ";
    output += errno_text(saved);
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }

  // The registry node is allocated before the fork, so registering it is a
  // splice that cannot throw while the locks are held.
  std::list<running_child> node;
  node.push_back(running_child{0, args.kill_tree, false});

  pid_t pid;
  int fork_errno = 0;
  {
    // Held (shared) across fork() so kill_all() cannot run between the fork
    // and the registration. The child inherits it held and never touches it.
    const std::shared_lock<std::shared_mutex> gate(fork_gate);
    pid = fork();
    fork_errno = errno;
    if (pid == 0) {
      // Child. Everything from here to execvp must be async-signal-safe: no
      // allocation, no locking, no libstdc++ calls that might do either.
      close(pipefd[0]);
      if (dup2(devnull, STDIN_FILENO) < 0) _exit(127);
      close(devnull);
      // dup2() clears close-on-exec on the new descriptor, so 0, 1 and 2
      // survive the exec while pipefd[1] and devnull themselves do not.
      if (dup2(pipefd[1], STDOUT_FILENO) < 0) _exit(127);
      if (dup2(pipefd[1], STDERR_FILENO) < 0) _exit(127);
      close(pipefd[1]);
      // `kill tree`: lead a session - and so a process group - of our own, so
      // that the parent can signal -pid and reach whatever the script forks,
      // backgrounds or leaves behind, rather than the script alone. A session
      // rather than just a group: it has no controlling terminal, so a script
      // that touches the tty fails instead of being stopped by SIGTTIN/SIGTTOU
      // as a background group of `nscp test`'s terminal would be. The cost is
      // that the script is already a session leader, so a setsid() call in
      // the script itself fails with EPERM (the setsid utility forks first and
      // is unaffected).
      if (args.kill_tree) setsid();
      close_above_stdio(max_fd);
      execvp(cargs[0], cargs.data());
      // execvp only returns on error.
      _exit(127);
    }
    if (pid > 0) {
      node.front().pid = pid;
      const std::lock_guard<std::mutex> lock(children_mutex);
      children.splice(children.end(), node);
    }
  }
  close(devnull);
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    output = "Failed to fork: ";
    output += errno_text(fork_errno);
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }

  // Parent.
  close(pipefd[1]);
  const child_registration_guard registered(pid);

  // Compute the effective timeout once so the deadline and the messages that
  // report it agree (a caller-supplied 0 falls back to 30s here).
  const unsigned int effective_timeout = args.timeout > 0 ? args.timeout : 30;
  const time_t deadline = time(nullptr) + effective_timeout;
  bool timed_out = false;
  bool had_error = false;
  output = drain_with_timeout(pipefd[0], deadline, timed_out, had_error);
  close(pipefd[0]);

  if (timed_out) {
    // With `kill tree` both signals go to the process group, so a helper the
    // script backgrounded - the usual reason the pipe never closed - dies too.
    terminate_child(pid, args.kill_tree);
    int status = 0;
    if (killed_by_unload(reap_child(pid, status), status)) {
      // kill_all() got there first; that, not the timeout, is why it ended.
      output.clear();
      return finish_child(args, status, true, output);
    }
    output = "Command " + args.alias + " didn't terminate within " + std::to_string(effective_timeout) + "s; killed";
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }

  if (had_error) {
    // The output can no longer be read, so whatever the script goes on to do
    // cannot become a result. End it now rather than wait for it with no
    // deadline: the timeout only bounded the drain.
    terminate_child(pid, args.kill_tree);
    int status = 0;
    const bool unloading = reap_child(pid, status);
    if (killed_by_unload(unloading, status)) {
      output.clear();
      return finish_child(args, status, true, output);
    }
    const std::string reason = "Command " + args.alias + " failed while its output was being read; killed";
    output = output.empty() ? reason : reason + "\n" + output;
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }

  if (wait_exited(pid, true) == wait_result::failed) {
    // The guard is about to drop the registration, after which nothing could
    // reach the child any more - neither a later unload nor a reap. So it is
    // killed here, on the same reasoning as terminate_child()'s SIGKILL.
    const int saved = errno;
    signal_child(pid, args.kill_tree, SIGKILL);
    output = "Failed to wait for " + args.alias + ": ";
    output += errno_text(saved);
    output += "; killed";
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  int status = 0;
  const bool unloading = reap_child(pid, status);
  return finish_child(args, status, unloading, output);
}

}  // namespace

int process::execute_process(const process::exec_arguments& args, std::string& output) {
  // The run-as settings (user/domain/password) are implemented by the Windows
  // launcher only (LogonUser + CreateProcessAsUser). This launcher never read
  // them, so a script an operator had sandboxed with `user = nobody` ran as the
  // service identity - root on a manual `nscp service` run - with a plaintext
  // password in the ini for nothing. Refuse rather than silently ignore: on
  // Linux the supported way to drop or raise privileges is sudo in the command
  // itself, which the operator grants in sudoers.
  if (!args.user.empty() || !args.domain.empty() || !args.password.empty()) {
    output = "Refusing to run " + args.alias +
             ": the user, domain and password settings are only supported on Windows; on Linux prefix the command with sudo (for example `command = "
             "sudo -n -u <user> /path/to/script`) and grant it in sudoers instead";
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  if (!args.argv.empty()) {
    return execute_argv(args, output);
  }
  // Legacy single-string command (no argv supplied). Run it through the shell,
  // but via the same fork/exec machinery as execute_argv rather than popen(),
  // so the timeout and output cap are enforced. popen() hid the child pid, so a
  // hung script blocked fread()/pclose() forever with `timeout=` silently
  // unenforced - a worker thread wedged per invocation. `/bin/sh -c <command>`
  // reproduces popen's semantics exactly (popen itself execs `/bin/sh -c`).
  process::exec_arguments shell_args = args;
  shell_args.argv = {"/bin/sh", "-c", args.command};
  return execute_argv(shell_args, output);
}
