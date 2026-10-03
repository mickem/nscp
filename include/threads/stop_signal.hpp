// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <chrono>
#include <exception>
#include <string>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#include <cerrno>
#endif

#include <error/error.hpp>
#include <threads/stop_requested.hpp>

namespace threads {

// One-shot "please stop" signal for a background worker, plus the primitive
// that worker blocks on. Windows uses a manual-reset event; POSIX uses a
// self-pipe whose read end can sit in a poll() set.
//
// This exists because three modules (CheckSystem's pdh_thread, and the
// CheckEventLog / CheckLogFile realtime threads) each open-coded the same
// create / guarded-signal / join / close sequence, drifted apart, and had to
// be fixed one at a time. The invariants worth having in one place are:
//
//   * create() reports failure. A worker that cannot be signalled cannot be
//     joined, so start() must refuse to spawn it rather than hang shutdown
//     later (poll() silently ignores a -1 fd; WaitForMultipleObjects returns
//     WAIT_FAILED for a null handle, and that failure hides the stop request
//     for every other handle in the same array).
//   * signal() on a closed signal is a silent no-op, not an error. stop() is
//     called from both unloadModule() and the destructor, and the second call
//     is a normal part of shutdown - it should not log.
//   * close() is idempotent and belongs in stop(), after the join, so a
//     stop/start cycle gets a fresh unsignalled primitive instead of leaking
//     one handle (or fd pair) per cycle.
//
// On Windows the event is deliberately UNNAMED. It used to be named
// "EventLogShutdown", and all three modules created that same name: one shared
// kernel object meant any one module stopping silently killed the others'
// threads, and a stop/start cycle reopened the still-signalled object so the
// restarted thread exited immediately. A name would also let any co-resident
// process signal it and disable monitoring from outside.
//
// stop_requested, which a worker throws when it abandons a step because this
// signal fired, lives in threads/stop_requested.hpp (included above) so that
// guarded_thread.hpp can recognise it without pulling <windows.h> in too.

class stop_signal {
 public:
  stop_signal() = default;
  ~stop_signal() { close(); }

  // The signal owns a kernel handle / fd pair, so it is move-only at most;
  // nothing needs to move one today, so both are simply disabled.
  stop_signal(const stop_signal &) = delete;
  stop_signal &operator=(const stop_signal &) = delete;

  // Create the primitive. Returns false and fills `error` with a formatted OS
  // message on failure; callers are expected to log it and not start a worker.
  bool create(std::string &error) {
    close();
#ifdef WIN32
    handle_ = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (handle_ == nullptr) {
      error = error::lookup::last_error();
      return false;
    }
#else
    // Close-on-exec from the moment the pipe exists, so a child the agent
    // spawns (exec_command, an external script) inherits neither end: one
    // that wrote to it would stop the worker for good. pipe2() sets the flag
    // atomically where it exists; elsewhere (macOS) fcntl() closes the window
    // as far as it can be closed, as execute_process_unix.cpp does.
#if defined(__linux__) && defined(O_CLOEXEC)
    int piped = ::pipe2(fds_, O_CLOEXEC);
#else
    int piped = ::pipe(fds_);
    if (piped == 0 && (::fcntl(fds_[0], F_SETFD, FD_CLOEXEC) != 0 || ::fcntl(fds_[1], F_SETFD, FD_CLOEXEC) != 0)) {
      const int saved_errno = errno;
      ::close(fds_[0]);
      ::close(fds_[1]);
      errno = saved_errno;
      piped = -1;
    }
#endif
    if (piped == -1) {
      const int saved_errno = errno;
      // POSIX leaves the array unspecified on failure; put it back to a state
      // valid() and close() recognise as "nothing here".
      fds_[0] = fds_[1] = -1;
      error = error::lookup::last_error(saved_errno);
      return false;
    }
#endif
    return true;
  }

  bool valid() const {
#ifdef WIN32
    return handle_ != nullptr;
#else
    return fds_[0] >= 0 && fds_[1] >= 0;
#endif
  }

  // Ask the worker to stop. A no-op when nothing was created or the signal is
  // already closed, so a repeated stop() stays quiet.
  void signal() const {
    if (!valid()) return;
#ifdef WIN32
    ::SetEvent(handle_);
#else
    // Best effort: the worker is going away either way, and a failed write on
    // an already-draining pipe is not worth reporting during shutdown.
    const ssize_t ignored = ::write(fds_[1], " ", 1);
    static_cast<void>(ignored);
#endif
  }

  // Release the primitive. Idempotent; call after joining the worker.
  void close() {
#ifdef WIN32
    if (handle_ != nullptr) {
      ::CloseHandle(handle_);
      handle_ = nullptr;
    }
#else
    for (int &fd : fds_) {
      if (fd >= 0) ::close(fd);
      fd = -1;
    }
#endif
  }

  enum class wait_result { signalled, timed_out, failed };

  // Block for up to `timeout`, or until signal() is called. `failed` leaves
  // the OS error in errno (GetLastError() on Windows) for the caller to
  // report; a signal that was never created fails too.
  //
  // The deadline is measured on steady_clock and each wait asks for what is
  // left of it, so neither a step of the wall clock (which stretches a
  // condition variable's timed wait on libc++) nor a signal handler
  // interrupting poll() (EINTR) changes how long it waits: an interrupted
  // wait resumes rather than ending early.
  wait_result wait_for(std::chrono::milliseconds timeout) const {
    if (!valid()) return wait_result::failed;
    const std::chrono::steady_clock::time_point due = std::chrono::steady_clock::now() + timeout;
    for (;;) {
      const auto remaining = due - std::chrono::steady_clock::now();
      if (remaining <= std::chrono::steady_clock::duration::zero()) return wait_result::timed_out;
      // Rounded up: truncating would end the wait up to a millisecond early.
      const auto left = std::chrono::ceil<std::chrono::milliseconds>(remaining);
#ifdef WIN32
      const DWORD rc = ::WaitForSingleObject(handle_, static_cast<DWORD>(left.count()));
      if (rc == WAIT_OBJECT_0) return wait_result::signalled;
      if (rc == WAIT_TIMEOUT) continue;
      return wait_result::failed;
#else
      struct pollfd fd = {fds_[0], POLLIN, 0};
      const int ready = ::poll(&fd, 1, static_cast<int>(left.count()));
      if (ready > 0) return wait_result::signalled;
      if (ready == 0 || errno == EINTR) continue;
      return wait_result::failed;
#endif
    }
  }

#ifdef WIN32
  // For WaitForSingleObject / WaitForMultipleObjects. Null when not created.
  HANDLE native_handle() const { return handle_; }
#else
  // The read end, for a poll()/select() set. -1 when not created.
  int wait_fd() const { return fds_[0]; }
#endif

 private:
#ifdef WIN32
  HANDLE handle_ = nullptr;
#else
  int fds_[2] = {-1, -1};
#endif
};

}  // namespace threads
