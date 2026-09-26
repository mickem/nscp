// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "exec_command.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>

namespace system_exec {

// Execute a program directly (no shell) and capture stdout. argv[0] is the
// program; remaining elements are arguments passed verbatim to execvp.
exec_result run(const std::vector<std::string> &argv, const int timeout_ms) {
  exec_result out;
  if (argv.empty()) return out;

  int pipefd[2];
  if (pipe(pipefd) == -1) return out;
  // Close-on-exec on both ends, so a child another thread forks at the same
  // time does not inherit the write end and hold our read open until it exits.
  // The dup2 onto stdout below clears the flag where it is wanted.
  fcntl(pipefd[0], F_SETFD, FD_CLOEXEC);
  fcntl(pipefd[1], F_SETFD, FD_CLOEXEC);

  // Build argv before fork(): a heap allocation in the child can block on a
  // lock another thread held at fork time, and the parent then blocks in
  // read() for good.
  std::vector<char *> cargv;
  cargv.reserve(argv.size() + 1);
  for (const auto &a : argv) cargv.push_back(const_cast<char *>(a.c_str()));
  cargv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid == -1) {
    close(pipefd[0]);
    close(pipefd[1]);
    return out;
  }

  if (pid == 0) {
    close(pipefd[0]);
    if (dup2(pipefd[1], STDOUT_FILENO) == -1) _exit(127);
    const int devnull = open("/dev/null", O_WRONLY);
    if (devnull != -1) {
      dup2(devnull, STDERR_FILENO);
      close(devnull);
    }
    close(pipefd[1]);

    // Only async-signal-safe calls between fork() and exec().
    execvp(cargv[0], cargv.data());
    _exit(127);
  }

  close(pipefd[1]);
  out.started = true;
  std::array<char, 4096> buffer{};
  // Bounded wait: a child that never exits (or never closes its stdout) must
  // not hang the check forever. The bound is an absolute deadline, not a fresh
  // timeout handed to every poll() - a child trickling one byte at a time reset
  // the budget on each iteration and was never killed.
  const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  bool timed_out = false;
  for (;;) {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      timed_out = true;
      break;
    }
    struct pollfd pfd;
    pfd.fd = pipefd[0];
    pfd.events = POLLIN;
    pfd.revents = 0;
    const int ready = poll(&pfd, 1, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count()));
    if (ready < 0 && errno == EINTR) continue;
    if (ready <= 0) {
      timed_out = true;
      break;
    }
    const ssize_t n = read(pipefd[0], buffer.data(), buffer.size());
    if (n < 0 && errno == EINTR) continue;
    if (n <= 0) break;
    out.output.append(buffer.data(), static_cast<size_t>(n));
  }
  close(pipefd[0]);

  int status = 0;
  if (timed_out) kill(pid, SIGKILL);
  pid_t reaped = -1;
  do {
    reaped = waitpid(pid, &status, 0);
  } while (reaped == -1 && errno == EINTR);
  out.timed_out = timed_out;
  // No exit status (the child was not reaped) is not a clean exit.
  if (reaped == pid && !timed_out && WIFEXITED(status)) out.exit_code = WEXITSTATUS(status);
  // 127 is what the child exits with when exec itself failed.
  if (out.exit_code == 127 && out.output.empty()) out.started = false;
  return out;
}

std::string exec_command(const std::vector<std::string> &argv, const int timeout_ms) { return run(argv, timeout_ms).output; }

}  // namespace system_exec
