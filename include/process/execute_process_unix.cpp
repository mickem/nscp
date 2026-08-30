// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <NSCAPI.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <algorithm>
#include <bytes/buffer.hpp>
#include <process/execute_process.hpp>
#include <string>
#include <vector>

#define BUFFER_SIZE 4096

// Upper bound on captured child output. A check is expected to print one Nagios
// line; without a cap a script (buggy or hostile) can emit hundreds of MB within
// its timeout window and balloon the service's memory. 8 MiB is far above any
// legitimate check output. Once reached we keep reading (so the child never
// blocks on a full pipe and the timeout stays enforceable) but discard the rest.
#define MAX_OUTPUT_BYTES (8u * 1024u * 1024u)

bool early_timeout = false;
typedef hlp::buffer<char> buffer_type;

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
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    const time_t now = time(nullptr);
    if (now >= deadline) {
      timed_out = true;
      return out;
    }
    struct timeval tv;
    tv.tv_sec = deadline - now;
    tv.tv_usec = 0;
    const int ready = select(fd + 1, &rfds, nullptr, nullptr, &tv);
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
    // Append up to the cap; past it keep draining but discard, so the child is
    // never blocked on a full pipe (which would defeat the timeout) yet memory
    // stays bounded.
    if (out.size() < MAX_OUTPUT_BYTES) {
      const std::size_t room = MAX_OUTPUT_BYTES - out.size();
      out.append(buffer.get(), std::min(static_cast<std::size_t>(n), room));
      if (out.size() >= MAX_OUTPUT_BYTES) out.append("\n[output truncated]");
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

  int pipefd[2];
  if (pipe(pipefd) != 0) {
    output = "Failed to create pipe: ";
    output += strerror(errno);
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }

  const pid_t pid = fork();
  if (pid < 0) {
    close(pipefd[0]);
    close(pipefd[1]);
    output = "Failed to fork: ";
    output += strerror(errno);
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  if (pid == 0) {
    // Child. Everything from here to execvp must be async-signal-safe: no
    // allocation, no locking, no libstdc++ calls that might do either.
    close(pipefd[0]);
    if (dup2(pipefd[1], STDOUT_FILENO) < 0) _exit(127);
    if (dup2(pipefd[1], STDERR_FILENO) < 0) _exit(127);
    close(pipefd[1]);
    execvp(cargs[0], cargs.data());
    // execvp only returns on error.
    _exit(127);
  }

  // Parent.
  close(pipefd[1]);

  const time_t deadline = time(nullptr) + (args.timeout > 0 ? args.timeout : 30);
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
        output = "Command " + args.alias + " didn't terminate within " + std::to_string(args.timeout) + "s; killed";
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
    output = "Command " + args.alias + " didn't terminate within " + std::to_string(args.timeout) + "s; killed";
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
    output += strerror(errno);
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  return map_exit_status(status);
}

}  // namespace

int process::execute_process(const process::exec_arguments& args, std::string& output) {
  early_timeout = false;
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
