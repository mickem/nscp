/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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

#include <bytes/buffer.hpp>
#include <process/execute_process.hpp>
#include <string>
#include <vector>

#define BUFFER_SIZE 4096

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
    buffer[static_cast<std::size_t>(n)] = 0;
    out.append(buffer.get(), static_cast<std::size_t>(n));
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
    // Child. Wire stdout / stderr to the parent's pipe and execvp.
    close(pipefd[0]);
    if (dup2(pipefd[1], STDOUT_FILENO) < 0) _exit(127);
    if (dup2(pipefd[1], STDERR_FILENO) < 0) _exit(127);
    close(pipefd[1]);
    // Build an argv compatible with execvp.
    std::vector<char*> cargs;
    cargs.reserve(args.argv.size() + 1);
    for (const auto& a : args.argv) {
      cargs.push_back(const_cast<char*>(a.c_str()));
    }
    cargs.push_back(nullptr);
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

// Legacy popen path, used when the caller did not supply argv. /bin/sh -c
// is involved, so any operator-controlled string still has to be carefully
// quoted by the caller. New callers should populate exec_arguments::argv.
int execute_popen(const process::exec_arguments& args, std::string& output) {
  FILE* fp = popen(args.command.c_str(), "r");
  if (fp == nullptr) {
    output = "NRPE: Call to popen() failed";
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  buffer_type buffer(BUFFER_SIZE);
  std::size_t bytes_read = 0;
  while ((bytes_read = fread(buffer.get(), 1, buffer.size() - 1, fp)) > 0) {
    if (bytes_read < BUFFER_SIZE) {
      buffer[bytes_read] = 0;
      output += std::string(buffer.get());
    }
  }
  const int status = pclose(fp);
  if (status == -1 || !WIFEXITED(status)) {
    return NSCAPI::query_return_codes::returnUNKNOWN;
  }
  return WEXITSTATUS(status);
}
}  // namespace

int process::execute_process(const process::exec_arguments& args, std::string& output) {
  early_timeout = false;
  if (!args.argv.empty()) {
    return execute_argv(args, output);
  }
  return execute_popen(args, output);
}
