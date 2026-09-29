// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread.hpp>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <threads/guarded_thread.hpp>

namespace check_ad {

// Runs a blocking call that has no deadline of its own (DsBindW, getaddrinfo)
// on a guarded worker, so the check can give up on it when its timeout runs
// out instead of being held open for as long as the OS keeps retrying.
//
// Windows cannot cancel those calls, so a worker that misses its deadline is
// not stopped: it stays parked here until the call returns by itself. Its body
// must therefore own everything it touches (capture shared_ptrs and copies,
// never references into the caller's frame), and the caller may read the
// body's results only after wait_until() returned true.
//
// Parking is per key. While a worker for a key is still running, start()
// refuses to start another one for it, so a check scheduled every minute
// against a black-holed host keeps at most one stuck thread instead of adding
// one per run.
class bounded_workers {
 public:
  // Completion is signalled through this rather than by joining: the caller
  // waits here, and the thread itself is joined only under the mutex, so no
  // two callers ever join the same boost::thread. The task does not hold its
  // thread - the thread's body holds the task, and the reverse would make a
  // cycle that keeps both alive.
  struct task {
    std::mutex mutex;
    std::condition_variable finished;
    bool done = false;

    void mark_done() {
      {
        std::lock_guard<std::mutex> lock(mutex);
        done = true;
      }
      finished.notify_all();
    }
    bool is_done() {
      std::lock_guard<std::mutex> lock(mutex);
      return done;
    }
  };
  typedef std::shared_ptr<task> handle;

  bounded_workers() = default;
  bounded_workers(const bounded_workers &) = delete;
  bounded_workers &operator=(const bounded_workers &) = delete;
  // Normally empty by now (shutdown() drains it); a worker still blocked is
  // let go rather than joined, since that could take as long as the OS call.
  ~bounded_workers() {
    for (auto &entry : workers_) release(entry.second);
  }

  // Start `body` under `key`, or return null when the previous worker for
  // that key has not returned yet. `name` is what a report of the worker
  // dying names it by.
  template <typename Body, typename Report>
  handle start(const std::string &key, const std::string &name, Body body, Report report) {
    std::lock_guard<std::mutex> lock(mutex_);
    prune_finished();
    if (workers_.count(key) != 0) return handle();
    handle t = std::make_shared<task>();
    std::shared_ptr<boost::thread> thread = threads::start_guarded_thread(
        name,
        [t, body]() mutable {
          // Marks the task done on every exit, an exception included: the
          // guard around this lambda catches it only after this has run.
          struct done_on_exit {
            handle t;
            ~done_on_exit() { t->mark_done(); }
          } marker{t};
          body();
        },
        report);
    workers_[key] = parked_worker{t, thread};
    return t;
  }

  // Wait for `t` until `deadline`; true when its body has returned.
  static bool wait_until(const handle &t, const std::chrono::steady_clock::time_point deadline) {
    std::unique_lock<std::mutex> lock(t->mutex);
    return t->finished.wait_until(lock, deadline, [&t] { return t->done; });
  }

  // Module unload: give each parked worker `grace` to return, then detach the
  // rest and call `on_abandon` once, which must keep the code they are still
  // running mapped (they return into this module when the OS call finishes).
  void shutdown(const std::chrono::milliseconds grace, const std::function<void()> &on_abandon) {
    std::map<std::string, parked_worker> parked;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      parked.swap(workers_);
    }
    const auto deadline = std::chrono::steady_clock::now() + grace;
    bool abandoned = false;
    for (auto &entry : parked) {
      if (!wait_until(entry.second.state, deadline)) abandoned = true;
      release(entry.second);
    }
    if (abandoned && on_abandon) on_abandon();
  }

  // Workers still parked (finished ones are dropped first). For tests.
  std::size_t parked() {
    std::lock_guard<std::mutex> lock(mutex_);
    prune_finished();
    return workers_.size();
  }

 private:
  struct parked_worker {
    handle state;
    std::shared_ptr<boost::thread> thread;
  };

  // Join a worker whose body has returned (it is only unwinding, so that is
  // immediate), detach one that has not.
  static void release(parked_worker &w) {
    if (w.state->is_done())
      w.thread->join();
    else
      w.thread->detach();
  }

  // Callers hold mutex_.
  void prune_finished() {
    for (auto it = workers_.begin(); it != workers_.end();) {
      if (it->second.state->is_done()) {
        release(it->second);
        it = workers_.erase(it);
      } else {
        ++it;
      }
    }
  }

  std::mutex mutex_;
  std::map<std::string, parked_worker> workers_;
};

// The module's one set of parked workers, drained by unloadModule().
inline bounded_workers &module_workers() {
  static bounded_workers workers;
  return workers;
}

}  // namespace check_ad
