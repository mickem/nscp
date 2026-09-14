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

bool early_timeout = false;
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

void process::kill_all() {
  // TODO: Fixme
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

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    output = "Failed to fork: ";
    output += errno_text(errno);
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  if (pid == 0) {
    // Child. Everything from here to execvp must be async-signal-safe: no
    // allocation, no locking, no libstdc++ calls that might do either.
    close(pipefd[0]);
    // dup2() clears close-on-exec on the new descriptor, so 1 and 2 survive
    // the exec while pipefd[1] itself does not.
    if (dup2(pipefd[1], STDOUT_FILENO) < 0) _exit(127);
    if (dup2(pipefd[1], STDERR_FILENO) < 0) _exit(127);
    close(pipefd[1]);
    close_above_stdio(max_fd);
    execvp(cargs[0], cargs.data());
    // execvp only returns on error.
    _exit(127);
  }

  // Parent.
  close(pipefd[1]);

  // Compute the effective timeout once so the deadline and the messages that
  // report it agree (a caller-supplied 0 falls back to 30s here).
  const unsigned int effective_timeout = args.timeout > 0 ? args.timeout : 30;
  const time_t deadline = time(nullptr) + effective_timeout;
  bool timed_out = false;
  bool had_error = false;
  output = drain_with_timeout(pipefd[0], deadline, timed_out, had_error);
  close(pipefd[0]);

  if (timed_out) {
    // Graceful first, hard second. Treat ECHILD/ESRCH as already-gone.
    kill(pid, SIGTERM);
    for (int i = 0; i < 20; ++i) {
      int status = 0;
      const pid_t r = waitpid(pid, &status, WNOHANG);
      if (r == pid) {
        output = "Command " + args.alias + " didn't terminate within " + std::to_string(effective_timeout) + "s; killed";
        return NSCAPI::query_return_codes::returnUNKNOWN;
      }
      if (r < 0 && errno != EINTR) break;
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = 100 * 1000 * 1000;  // 100ms
      nanosleep(&ts, nullptr);
    }
    kill(pid, SIGKILL);
    int status = 0;
    waitpid(pid, &status, 0);
    output = "Command " + args.alias + " didn't terminate within " + std::to_string(effective_timeout) + "s; killed";
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }

  if (had_error) {
    // Child probably crashed. Reap and report.
    int status = 0;
    waitpid(pid, &status, 0);
    if (output.empty()) {
      output = "Command " + args.alias + " failed during read";
    }
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }

  int status = 0;
  if (waitpid(pid, &status, 0) < 0) {
    output = "Failed to wait for child: ";
    output += errno_text(errno);
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  return map_exit_status(status);
}

}  // namespace

int process::execute_process(const process::exec_arguments& args, std::string& output) {
  early_timeout = false;
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
