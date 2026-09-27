// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "exec_command.h"

#include <fcntl.h>
#include <poll.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace system_exec {
namespace {
struct pipe_pair {
  int reader = -1, writer = -1;
  bool open() {
    int fds[2];
    if (pipe(fds) == -1) return false;
    reader = fds[0];
    writer = fds[1];
    // Do not let another thread's exec keep our pipe open.
    return fcntl(reader, F_SETFD, FD_CLOEXEC) != -1 && fcntl(writer, F_SETFD, FD_CLOEXEC) != -1;
  }
  void close_reader() {
    if (reader != -1) close(reader);
    reader = -1;
  }
  void close_writer() {
    if (writer != -1) close(writer);
    writer = -1;
  }
  ~pipe_pair() {
    close_reader();
    close_writer();
  }
};
}  // namespace

exec_result run(const std::vector<std::string> &argv, const int timeout_ms, const std::size_t max_output, const bool capture_stderr) {
  exec_result out;
  if (argv.empty()) {
    out.failure = "no command provided";
    return out;
  }

  pipe_pair output, errors, exec_status;
  if (!output.open() || !exec_status.open() || (capture_stderr && !errors.open())) {
    out.error_number = errno;
    out.failure = "pipe setup failed";
    return out;
  }
  // This pipe carries errno on exec failure; successful exec closes it.
  // Exit code 127 alone could also come from a program that actually ran.
  if (fcntl(exec_status.reader, F_SETFL, O_NONBLOCK) == -1) {
    out.error_number = errno;
    out.failure = "pipe setup failed";
    return out;
  }
  // Allocate before fork: another thread may hold the allocator lock.
  std::vector<char *> cargv;
  for (const auto &a : argv) cargv.push_back(const_cast<char *>(a.c_str()));
  cargv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid == -1) {
    out.error_number = errno;
    out.failure = "fork failed";
    return out;
  }
  if (pid == 0) {
    // Only async-signal-safe operations between fork and exec.
    const auto fail = [&]() {
      const int reason = errno;
      ssize_t written;
      do {
        written = write(exec_status.writer, &reason, sizeof(reason));
      } while (written == -1 && errno == EINTR);
      _exit(127);
    };
    output.close_reader();
    errors.close_reader();
    exec_status.close_reader();
    if (dup2(output.writer, STDOUT_FILENO) == -1) fail();
    if (capture_stderr) {
      if (dup2(errors.writer, STDERR_FILENO) == -1) fail();
    } else {
      const int devnull = ::open("/dev/null", O_WRONLY);
      if (devnull == -1) fail();
      if (dup2(devnull, STDERR_FILENO) == -1) fail();
      close(devnull);
    }
    output.close_writer();
    errors.close_writer();
    execvp(cargv[0], cargv.data());
    fail();
  }

  output.close_writer();
  errors.close_writer();
  exec_status.close_writer();
  out.started = true;
  std::array<char, 4096> buffer{};
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while ((output.reader != -1 || errors.reader != -1) && !out.output_failed) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      out.timed_out = true;
      break;
    }
    struct pollfd fds[2] = {{output.reader, POLLIN, 0}, {errors.reader, POLLIN, 0}};
    const int ready = poll(fds, 2, static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now).count()));
    if (ready < 0 && errno == EINTR) continue;
    if (ready < 0) {
      out.error_number = errno;
      out.failure = "poll failed";
      out.output_failed = true;
      break;
    }
    if (ready == 0) {
      out.timed_out = true;
      break;
    }
    for (int i = 0; i < 2; ++i) {
      if (!fds[i].revents) continue;
      const ssize_t n = read(fds[i].fd, buffer.data(), buffer.size());
      if (n < 0 && errno == EINTR) continue;
      if (n < 0) {
        out.error_number = errno;
        out.failure = i == 0 ? "stdout read failed" : "stderr read failed";
        out.output_failed = true;
        break;
      }
      if (n == 0) {
        (i == 0 ? output : errors).close_reader();
      } else if (i == 1) {
        // Drain after the diagnostic buffer fills, so stderr cannot block the
        // child or consume unbounded memory.
        out.error_output.append(buffer.data(), std::min(static_cast<std::size_t>(n), 4096 - out.error_output.size()));
      } else if (max_output && static_cast<std::size_t>(n) > max_output - out.output.size()) {
        out.output_failed = out.output_limit_exceeded = true;
        out.failure = "stdout limit exceeded (" + std::to_string(max_output) + " bytes)";
        break;
      } else {
        out.output.append(buffer.data(), static_cast<std::size_t>(n));
      }
    }
  }
  output.close_reader();
  errors.close_reader();

  // EOF is not child exit. Keep the same deadline if it closes its output and
  // lingers, rather than blocking forever in waitpid.
  int status = 0;
  pid_t reaped = 0;
  while (!out.timed_out && !out.output_failed) {
    reaped = waitpid(pid, &status, WNOHANG);
    if (reaped == pid) break;
    if (reaped == -1 && errno != EINTR) {
      out.error_number = errno;
      out.failure = "waitpid failed";
      break;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      out.timed_out = true;
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  if (out.timed_out || out.output_failed) {
    kill(pid, SIGKILL);
    do {
      reaped = waitpid(pid, &status, 0);
    } while (reaped == -1 && errno == EINTR);
  }
  if (reaped == pid && WIFEXITED(status)) out.exit_code = WEXITSTATUS(status);
  int exec_errno = 0;
  ssize_t n;
  do {
    n = read(exec_status.reader, &exec_errno, sizeof(exec_errno));
  } while (n == -1 && errno == EINTR);
  if (n == sizeof(exec_errno)) {
    out.started = false;
    out.error_number = exec_errno;
    out.failure = "exec or child setup failed";
  }
  return out;
}

std::string exec_command(const std::vector<std::string> &argv, const int timeout_ms) { return run(argv, timeout_ms).output; }

std::string run_inventory_command(const std::vector<std::string> &argv, const int timeout_ms, const std::size_t max_output) {
  const exec_result result = run(argv, timeout_ms, max_output, true);
  if (!result.started || result.timed_out || result.output_failed || !result.failure.empty() || result.exit_code != 0) {
    std::ostringstream message;
    message << "Inventory command";
    for (const auto &arg : argv) message << ' ' << std::quoted(arg);
    message << ": ";
    if (result.timed_out)
      message << "timed out after " << timeout_ms << " ms";
    else if (!result.failure.empty())
      message << result.failure;
    else
      message << "nonzero exit or signal termination";
    message << "; exit_code=" << result.exit_code;
    if (result.error_number) message << "; " << std::strerror(result.error_number);
    if (!result.error_output.empty()) {
      message << "; stderr: " << result.error_output;
      if (result.error_output.size() == 4096) message << " [stderr limited to 4096 bytes]";
    }
    throw std::runtime_error(message.str());
  }
  return result.output;
}
}  // namespace system_exec
