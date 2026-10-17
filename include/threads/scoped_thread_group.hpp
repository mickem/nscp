// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/thread.hpp>
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <threads/guarded_thread.hpp>
#include <vector>

/**
 * RAII-owning group of background worker threads.
 *
 *   * Threads are joined on destruction.
 *   * interrupt_all() delivers boost::thread_interrupted to threads parked
 *     on an interruption point; it does not preempt running code.
 *   * Thread bodies that throw (boost::thread_interrupted, std::exception,
 *     or anything else) are caught by threads::run_guarded() so an escaping
 *     exception cannot terminate the process. What escaped is reported
 *     through set_error_reporter(), which is handed the finished line
 *     threads::detail::render_thread_event() wrote: swallowing it silently
 *     left a pool quietly short of workers with nothing in the log to say
 *     why.
 *   * count() returns the number of currently-live worker threads.
 *   * wait_all_for() joins with a deadline and detaches whatever is still
 *     running at it, for a shutdown that must finish even if a worker hung.
 *
 * Non-copyable.
 *
 * Thread-safety:
 *   wait_all() and wait_all_for() forget the threads they joined (or gave up
 *   on), so the destructor calling wait_all() after a prior explicit one
 *   (e.g. from a shutdown sequence) is a safe no-op.
 *
 *   create_thread() blocks while a wait is in flight. A worker must therefore
 *   never create a thread in its own group while another thread may be
 *   joining it - it would wait for the join that waits for it; the scheduler
 *   keeps its watchdog off that path with a try_lock.
 */
class scoped_thread_group {
 public:
  scoped_thread_group() : state_(std::make_shared<shared_state>()) {}
  ~scoped_thread_group() { wait_all(); }

  scoped_thread_group(const scoped_thread_group&) = delete;
  scoped_thread_group& operator=(const scoped_thread_group&) = delete;

  typedef std::function<void(const std::string& /*message*/)> error_reporter;

  // How the death of a worker is reported. Set it before the first
  // create_thread(): a live worker reads it from its catch path without any
  // lock, so replacing it while the pool is running is a data race on a
  // std::function - and a reporter is typically re-set from a settings notify
  // callback, which re-runs on every reload. The assert is there because that
  // is exactly how the scheduler got it wrong. With none set an escaping
  // exception is still contained, just not logged.
  void set_error_reporter(error_reporter reporter) {
    assert(state_->live_count.load(std::memory_order_relaxed) == 0 && "set_error_reporter() must not race live workers; set it before create_thread()");
    state_->reporter = reporter;
  }

  template <typename Callable>
  void create_thread(Callable f, const std::string& name = "worker") {
    boost::mutex::scoped_lock lock(mutex_);
    // The body holds the shared state rather than `this`: a thread that
    // wait_all_for() gave up on outlives the group, and must still have its
    // counter and reporter to come back to.
    const std::shared_ptr<shared_state> state = state_;
    state->live_count.fetch_add(1, std::memory_order_relaxed);
    try {
      threads_.push_back(std::unique_ptr<boost::thread>(new boost::thread([state, f, name]() mutable {
        threads::run_guarded(name, f, [state](const std::string& message) {
          if (state->reporter) state->reporter(message);
        });
        state->live_count.fetch_sub(1, std::memory_order_relaxed);
      })));
    } catch (...) {
      // Spawn failed (e.g. boost::thread_resource_error, std::bad_alloc);
      // roll back the optimistic counter increment.
      state->live_count.fetch_sub(1, std::memory_order_relaxed);
      throw;
    }
  }

  void interrupt_all() {
    boost::mutex::scoped_lock lock(mutex_);
    for (const std::unique_ptr<boost::thread>& t : threads_) t->interrupt();
  }

  void wait_all() {
    boost::mutex::scoped_lock lock(mutex_);
    for (const std::unique_ptr<boost::thread>& t : threads_) {
      if (t->joinable()) t->join();
    }
    threads_.clear();
  }

  // Join every thread, giving up at `timeout` after the call. A thread still
  // running then is detached and forgotten - nothing can stop it from the
  // outside, and joining it again from the destructor would only move the
  // hang there. Returns how many were given up on (0: all joined). The body
  // keeps running against whatever it captured; it is the caller's job to
  // decide what that means.
  std::size_t wait_all_for(const boost::posix_time::time_duration timeout) {
    boost::mutex::scoped_lock lock(mutex_);
    const boost::system_time deadline = boost::get_system_time() + timeout;
    std::size_t abandoned = 0;
    for (const std::unique_ptr<boost::thread>& t : threads_) {
      if (!t->joinable()) continue;
      if (!t->timed_join(deadline)) {
        t->detach();
        ++abandoned;
      }
    }
    threads_.clear();
    return abandoned;
  }

  std::size_t count() const { return state_->live_count.load(std::memory_order_relaxed); }

 private:
  struct shared_state {
    std::atomic<std::size_t> live_count{0};
    error_reporter reporter;
  };
  // Held across every join, as boost::thread_group held its lock: a thread
  // created while a join is in flight waits for it instead of becoming an
  // orphan the join misses.
  boost::mutex mutex_;
  std::vector<std::unique_ptr<boost::thread>> threads_;
  std::shared_ptr<shared_state> state_;
};
