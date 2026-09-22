// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/thread.hpp>
#include <cassert>
#include <cstddef>
#include <functional>
#include <string>
#include <threads/guarded_thread.hpp>

/**
 * RAII-owning group of background worker threads.
 *
 *   * Threads are joined on destruction.
 *   * interrupt_all() delivers boost::thread_interrupted to threads parked
 *     on an interruption point; it does not preempt running code.
 *   * Thread bodies that throw (boost::thread_interrupted, std::exception,
 *     or anything else) are caught by threads::run_guarded() so an escaping
 *     exception cannot terminate the process. What escaped is reported
 *     through set_error_reporter(): swallowing it silently left a pool
 *     quietly short of workers with nothing in the log to say why.
 *   * count() returns the number of currently-live worker threads.
 *
 * Non-copyable.
 *
 * Thread-safety:
 *   wait_all() and interrupt_all() are idempotent — boost::thread_group's
 *   join_all() / interrupt_all() check joinable() per thread before acting,
 *   so the destructor calling wait_all() after a prior explicit wait_all()
 *   (e.g. from a shutdown sequence) is a safe no-op.
 *
 *   Precondition: create_thread() must not race with wait_all() from another
 *   thread. boost::thread_group's internal locking keeps the container
 *   consistent, but a thread created while wait_all() is in flight may
 *   become an orphan that the in-flight join misses. All current callers
 *   funnel start/stop through a single owning thread, so this is a documented
 *   precondition rather than an enforced one.
 */
class scoped_thread_group {
 public:
  scoped_thread_group() = default;
  ~scoped_thread_group() { wait_all(); }

  scoped_thread_group(const scoped_thread_group&) = delete;
  scoped_thread_group& operator=(const scoped_thread_group&) = delete;

  typedef std::function<void(const std::string& /*name*/, const std::string& /*detail*/)> error_reporter;

  // How the death of a worker is reported. Set it before the first
  // create_thread(): a live worker reads it from its catch path without any
  // lock, so replacing it while the pool is running is a data race on a
  // std::function - and a reporter is typically re-set from a settings notify
  // callback, which re-runs on every reload. The assert is there because that
  // is exactly how the scheduler got it wrong. With none set an escaping
  // exception is still contained, just not logged.
  void set_error_reporter(error_reporter reporter) {
    assert(live_count_.load(std::memory_order_relaxed) == 0 && "set_error_reporter() must not race live workers; set it before create_thread()");
    reporter_ = reporter;
  }

  template <typename Callable>
  void create_thread(Callable f, const std::string& name = "worker") {
    live_count_.fetch_add(1, std::memory_order_relaxed);
    try {
      group_.create_thread([this, f, name]() mutable {
        threads::run_guarded(name, f, [this](const std::string& n, const std::string& detail) {
          if (reporter_) reporter_(n, detail);
        });
        live_count_.fetch_sub(1, std::memory_order_relaxed);
      });
    } catch (...) {
      // Spawn failed (e.g. boost::thread_resource_error, std::bad_alloc);
      // roll back the optimistic counter increment.
      live_count_.fetch_sub(1, std::memory_order_relaxed);
      throw;
    }
  }

  void interrupt_all() { group_.interrupt_all(); }

  void wait_all() { group_.join_all(); }

  std::size_t count() const { return live_count_.load(std::memory_order_relaxed); }

 private:
  boost::thread_group group_;
  std::atomic<std::size_t> live_count_{0};
  error_reporter reporter_;
};
