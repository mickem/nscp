// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/date_time/gregorian/gregorian.hpp>
#include <scheduler/simple_scheduler.hpp>
#include <str/utf8.hpp>

boost::posix_time::ptime time_t_epoch(boost::gregorian::date(1970, 1, 1));

namespace simple_scheduler {

bool scheduler::has_metrics() const { return true; }

int scheduler::get_metric_executed() const { return static_cast<int>(metric_executed_.load()); }
int scheduler::get_metric_compleated() const { return static_cast<int>(metric_completed_.load()); }
int scheduler::get_metric_errors() const { return static_cast<int>(metric_errors_.load()); }
// The configured pool size. It no longer over-reports: the watchdog used to
// raise this without spawning anything, because it compared the target against
// threads_.count(), which counts the watchdog itself and so found the pool
// already full. spawn_missing_locked() counts workers now, so the target and
// the pool agree again.
std::size_t scheduler::get_metric_threads() const { return thread_count_; }
std::size_t scheduler::get_metric_ql() { return queue_.size(); }
int scheduler::get_avg_time() const {
  const std::uint64_t c = metric_count_.load();
  if (c == 0) {
    return 0;
  }
  // No reset: the accumulators are 64-bit, so there is nothing to wrap and
  // nothing to zero out from under the workers adding to them.
  return static_cast<int>(metric_time_ms_.load() / c);
}

int scheduler::get_metric_rate() const {
  const boost::posix_time::time_duration diff = now() - time_t_epoch;
  const std::uint64_t start = metric_start_.load();
  const std::uint64_t seconds = static_cast<std::uint64_t>(diff.total_seconds());
  if (start == 0 || seconds <= start) {
    return 0;
  }
  return static_cast<int>(metric_completed_.load() / (seconds - start));
}

void scheduler::start() {
  boost::posix_time::time_duration diff = now() - time_t_epoch;
  metric_start_ = static_cast<std::uint64_t>(diff.total_seconds());
  log_trace(__FILE__, __LINE__, "starting all threads");
  {
    // Opening the door is the starter's job alone. start_threads() used to
    // clear stop_requested_ as well, which let the watchdog re-open it while
    // stop() was joining.
    boost::mutex::scoped_lock l(pool_mutex_);
    running_ = true;
    stop_requested_ = false;
  }
  start_threads();
  log_trace(__FILE__, __LINE__, "Thread pool contains: " + str::xtos(threads_.count()));
}

void scheduler::prepare_shutdown() {
  log_trace(__FILE__, __LINE__, "prepare to shutdown");
  {
    boost::mutex::scoped_lock l(pool_mutex_);
    running_ = false;
    stop_requested_ = true;
    has_watchdog_ = false;
  }
  threads_.interrupt_all();
}
void scheduler::stop() {
  log_trace(__FILE__, __LINE__, "stopping all threads");
  {
    // Held across the join, not just the flag writes. boost::thread_group
    // keeps a shared lock for the whole of join_all() while create_thread
    // wants it exclusively, so a thread spawned during the join blocks in
    // create_thread and is never joined - and the join waits for it. Holding
    // pool_mutex_ here means nothing can reach create_thread until the pool is
    // gone. The watchdog, which is one of the threads being joined, only ever
    // try_locks this mutex (see scale_up), so it cannot be the thread we are
    // waiting for.
    boost::mutex::scoped_lock l(pool_mutex_);
    running_ = false;
    stop_requested_ = true;
    has_watchdog_ = false;
    threads_.interrupt_all();
    threads_.wait_all();
    // The pool is empty again, but the configured size stays: start() reuses
    // it, and zeroing it here meant a restarted scheduler had no workers until
    // something called set_threads() again.
    spawned_workers_ = 0;
  }
  log_trace(__FILE__, __LINE__, "Thread pool contains: " + str::xtos(threads_.count()));
}

int scheduler::add_task(const std::string &tag, const boost::posix_time::time_duration duration, const double jitter_factor, const bool schedule_first_run) {
  task item(tag, duration, jitter_factor);
  {
    boost::mutex::scoped_lock l(mutex_);
    item.id = ++schedule_id_;
    tasks_[item.id] = item;
  }
  if (schedule_first_run) reschedule(item, now());
  return item.id;
}
int scheduler::add_task(const std::string &tag, const cron_parser::schedule &schedule, const bool schedule_first_run) {
  task item(tag, schedule);
  {
    boost::mutex::scoped_lock l(mutex_);
    item.id = ++schedule_id_;
    tasks_[item.id] = item;
  }
  if (schedule_first_run) reschedule(item, now());
  return item.id;
}
void scheduler::run_now(const int id, const boost::posix_time::time_duration delay) {
  const op_task_object item = get_task(id);
  if (!item) {
    log_error(__FILE__, __LINE__, "Cannot run unknown task: " + str::xtos(id));
    return;
  }
  reschedule_at(item.value().tag, id, now() + delay, true);
}
void scheduler::remove_task(const int id) {
  boost::mutex::scoped_lock l(mutex_);
  const auto it = tasks_.find(id);
  // erase(end()) is undefined. The id is not guaranteed to be present: a task
  // can be queued twice (thread_proc reschedules on its catch-all path), so two
  // workers can both decide to remove it, and schedules_handler calls this
  // whenever a handler returns false.
  if (it == tasks_.end()) return;
  tasks_.erase(it);
}
scheduler::op_task_object scheduler::get_task(const int id) {
  boost::mutex::scoped_lock l(mutex_);
  const auto it = tasks_.find(id);
  if (it == tasks_.end()) return {};
  return {it->second};
}

void scheduler::clear_tasks() {
  {
    boost::mutex::scoped_lock l(mutex_);
    tasks_.clear();
  }
  // Drop the pending instances as well: without a task behind them they would
  // only produce "Task not found" errors as they come due, and on a reload
  // they would race the freshly added tasks.
  if (!queue_.clear()) log_error(__FILE__, __LINE__, "Failed to clear the schedule queue");
}

void scheduler::watch_dog(const int id) {
  bool maximum_threads_reached = false;
  while (!stop_requested_) {
    try {
      try {
        schedule_queue_type::value_type instance = queue_.top();
        if (instance) {
          boost::posix_time::time_duration off = now() - instance.value().time;
          if (off.total_seconds() > 5) {
            if (thread_count_ < 10) {
              thread_count_++;
              scale_up();
            } else if (!maximum_threads_reached) {
              log_error(
                  __FILE__, __LINE__,
                  "Auto-scaling of scheduler failed (maximum of 10 threads reached) you need to manually configure threads to resolve items running slow");
              maximum_threads_reached = true;
            }
          }
        }
      } catch (const std::exception &e) {
        log_error(__FILE__, __LINE__, "Watchdog issue: " + utf8::utf8_from_native(e.what()));
      } catch (...) {
        log_error(__FILE__, __LINE__, "Watchdog issue");
      }

      boost::thread::sleep(boost::get_system_time() + boost::posix_time::seconds(5));
    } catch (const boost::thread_interrupted &) {
      break;
    } catch (const std::exception &e) {
      log_error(__FILE__, __LINE__, "Watchdog issue: " + utf8::utf8_from_native(e.what()));
      break;
    } catch (...) {
      log_error(__FILE__, __LINE__, "Watchdog issue");
      break;
    }
  }
  log_trace(__FILE__, __LINE__, "Terminating thread: " + str::xtos(id));
}

void scheduler::thread_proc(const int id) {
  try {
    while (!stop_requested_) {
      schedule_queue_type::value_type instance = queue_.pop();
      if (!instance) {
        boost::unique_lock<boost::mutex> lock(idle_thread_mutex_);
        // Wait with a predicate that re-checks the queue. This closes the
        // lost-wakeup window where reschedule_at() pushed an item and notified
        // in the gap between the empty pop() above and this wait (the predicate
        // is evaluated on entry, before blocking). The timeout bounds any wakeup
        // that is still somehow missed to <=1s rather than blocking indefinitely.
        idle_thread_cond_.timed_wait(lock, boost::get_system_time() + boost::posix_time::seconds(1),
                                     [this]() { return stop_requested_ || !queue_.empty(); });
        continue;
      }

      try {
        boost::posix_time::time_duration off = now() - instance.value().time;
        if (!instance.value().suppress_late_warning && off.total_seconds() > error_threshold_) {
          log_error(__FILE__, __LINE__,
                    "Ran scheduled item " + instance.value().tag + "(" + str::xtos(instance.value().schedule_id) + ") " + str::xtos(off.total_seconds()) +
                        " seconds to late from thread " + str::xtos(id));
        }
        const boost::posix_time::time_duration wait = instance.value().time - now();
        if (wait.total_microseconds() > 0) {
          boost::this_thread::sleep(wait);
        }
      } catch (const boost::thread_interrupted &) {
        if (!queue_.push(instance.value())) log_error(__FILE__, __LINE__, "Failed to push item");
        // Either flag means the pool is going away: the interrupt consumed the
        // request, so a worker that only checked stop_requested_ would keep
        // looping forever if it were cleared between the two.
        if (stop_requested_ || !running_) {
          log_trace(__FILE__, __LINE__, "Terminating thread: " + str::xtos(id));
          return;
        }
        continue;
      } catch (...) {
        if (!queue_.push(instance.value())) {
          metric_errors_++;
          log_error(__FILE__, __LINE__, "Failed to push item");
        }
        continue;
      }

      boost::posix_time::ptime now_time = now();
      metric_executed_++;
      op_task_object item = get_task(instance.value().schedule_id);
      if (item) {
        try {
          bool to_reschedule = false;
          // Snapshot once: a second read could observe a different value, and
          // the null check has to apply to the pointer we actually call.
          if (handler *h = handler_.load()) {
            to_reschedule = h->handle_schedule(item.value());
          }
          boost::posix_time::time_duration duration = now() - now_time;

          metric_time_ms_ += static_cast<std::uint64_t>(duration.total_milliseconds());
          metric_count_++;
          if (to_reschedule) {
            reschedule(item.value(), now_time);
            metric_completed_++;
          } else {
            metric_errors_++;
            log_trace(__FILE__, __LINE__, "Abandoning: " + item.value().to_string());
          }
        } catch (...) {
          metric_errors_++;
          log_error(__FILE__, __LINE__, "UNKNOWN ERROR RUNNING TASK: " + item.value().tag);
          reschedule(item.value(), now_time);
        }
      } else {
        metric_errors_++;
        log_error(__FILE__, __LINE__, "Task not found: " + str::xtos(instance.value().schedule_id));
      }
    }
  } catch (const boost::thread_interrupted &) {
  } catch (const std::exception &e) {
    metric_errors_++;
    log_error(__FILE__, __LINE__, "Exception in scheduler thread (thread will be killed): " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    metric_errors_++;
    log_error(__FILE__, __LINE__, "Exception in scheduler thread (thread will be killed)");
  }
  log_trace(__FILE__, __LINE__, "Terminating thread: " + str::xtos(id));
}

void scheduler::reschedule(const task &item, boost::posix_time::ptime now_time) {
  if (item.is_disabled()) {
    log_error(__FILE__, __LINE__, "Found disabled task: " + item.to_string());
  } else {
    reschedule_at(item.tag, item.id, item.get_next(now_time));
  }
}
void scheduler::reschedule_at(const std::string &tag, const int id, boost::posix_time::ptime new_time, const bool suppress_late_warning) {
  schedule_instance instance;
  instance.tag = tag;
  instance.schedule_id = id;
  instance.time = new_time;
  instance.suppress_late_warning = suppress_late_warning;
  if (!queue_.push(instance)) {
    log_error(__FILE__, __LINE__, "Failed to reschedule item");
  }
  // Notify under idle_thread_mutex_ so the wakeup cannot be lost in the window
  // between a worker evaluating its wait predicate and actually blocking.
  {
    boost::unique_lock<boost::mutex> lock(idle_thread_mutex_);
    idle_thread_cond_.notify_one();
  }
}

void scheduler::start_threads() {
  boost::mutex::scoped_lock l(pool_mutex_);
  spawn_missing_locked();
}

void scheduler::scale_up() {
  // Called by the watchdog, which is itself one of the threads stop() joins.
  // It must never block on pool_mutex_: stop() holds that across the join, so
  // waiting for it here would be waiting for the thread that is waiting for
  // us. Missing a scale-up tick costs nothing - the next one is five seconds
  // away, and a stop in flight means there is nothing to scale.
  boost::mutex::scoped_try_lock l(pool_mutex_);
  if (!l.owns_lock()) return;
  spawn_missing_locked();
}

void scheduler::spawn_missing_locked() {
  if (!running_ || stop_requested_) return;
  // Against spawned_workers_, not threads_.count(): the latter also counts the
  // watchdog, so the first scale-up found the pool already "full" and only
  // raised the target.
  const std::size_t target = thread_count_;
  const std::size_t running = spawned_workers_;
  if (target > running) {
    for (std::size_t i = running; i < target; i++) {
      const int id = static_cast<int>(100 + i);
      const boost::function<void()> f = [this, id]() { this->thread_proc(id); };
      threads_.create_thread(f);
      spawned_workers_++;
    }
  }
  if (!has_watchdog_) {
    has_watchdog_ = true;
    const boost::function<void()> f = [this]() { this->watch_dog(0); };
    threads_.create_thread(f);
  }
}
}  // namespace simple_scheduler