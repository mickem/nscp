// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "nsclient_logger.hpp"

#include <algorithm>
#include <iostream>
#include <nscapi/settings/helper.hpp>
#include <nsclient/logger/logger_helper.hpp>
#include <threads/guarded_thread.hpp>

#include "../libs/settings_manager/settings_manager_impl.h"

namespace sh = nscapi::settings_helper;

namespace nsclient {
namespace logging {
namespace impl {

// Console everywhere, on every platform. Writing to a log file is a thing the
// *service* does - it selects the file explicitly when it starts (see
// cli_parser::parse_service, and the --log-backend in the systemd unit).
// Nothing about a one-shot command needs a log file, and needing write access
// to the install directory to run one is worse than useless.
nsclient_logger::nsclient_logger() : delivery_(std::make_shared<delivery>()) {}

nsclient_logger::~nsclient_logger() { nsclient_logger::destroy(); }

bool &nsclient_logger::on_worker_thread() {
  static thread_local bool on_worker = false;
  return on_worker;
}

bool nsclient_logger::is_console_option(const std::string &key) {
  return key == "console" || key == "no-console" || key == "oneline" || key == "no-std-err";
}

// "no-console" is how a module that has taken the console over for itself
// asks the core to stop writing to it (see NSAPISetLogOption and the
// CommandClient prompt, which renders log messages through its line editor
// so they do not land in the middle of what the user is typing).
void nsclient_logger::set_log_level(const std::string level) {
  if (!is_console_option(level)) {
    logger_impl::set_log_level(level);
    return;
  }
  {
    boost::lock_guard<boost::mutex> lock(sink_mutex_);
    if (level == "console")
      console_ = true;
    else if (level == "no-console")
      console_ = false;
    else if (level == "oneline")
      oneline_ = true;
    else
      no_std_err_ = true;
  }
  // A module handing the console back (the CommandClient prompt closing)
  // still has to see the lines logged while it held it: the core did not
  // print them, so they reach the screen through its handler or not at all.
  if (level == "console") flush_handlers();
}

// Wait, bounded, until every queued line has been handed to the handlers.
// Not on the worker itself, and never with sink_mutex_ held: a handler that
// logs takes it.
void nsclient_logger::flush_handlers() {
  if (on_worker_thread()) return;
  boost::unique_lock<boost::mutex> lock(delivery_->mutex);
  wait_for_queued(lock, boost::get_system_time() + boost::posix_time::milliseconds(delivery_wait_.count()));
}

// Only the lines already queued are waited for, so a flush ends even while
// other threads keep logging.
void nsclient_logger::wait_for_queued(boost::unique_lock<boost::mutex> &lock, const boost::system_time &deadline) {
  if (on_worker_thread()) return;
  const std::uint64_t target = delivery_->queued;
  delivery_->changed.timed_wait(lock, deadline, [&]() { return delivery_->stopping || delivery_->done >= target; });
}

void nsclient_logger::drop_queued() {
  delivery_->done += delivery_->queue.size();
  delivery_->queue.clear();
  delivery_->changed.notify_all();
}

void nsclient_logger::set_backend(const std::string backend) {
  boost::lock_guard<boost::mutex> lock(sink_mutex_);
  if (backend == "file" || backend == "threaded-file") {
    if (!file_) file_ = std::make_unique<simple_file_logger>("nsclient.log");
  } else {
    file_.reset();
  }
}

void nsclient_logger::write_sinks(const std::string &data) {
  boost::lock_guard<boost::mutex> lock(sink_mutex_);
  if (console_) {
    const std::pair<bool, std::string> m = logger_helper::render_console_message(oneline_, data);
    if (!no_std_err_ && m.first)
      std::cerr << m.second;
    else
      // Flush every line: on Windows nothing else flushes std::cout until the
      // next read from std::cin, so in `nscp test` the log only caught up
      // when a key was pressed.
      std::cout << m.second << std::flush;
  }
  if (file_) file_->do_log(data);
}

void nsclient_logger::do_log(const std::string data) {
  write_sinks(data);
  // A line from inside a handler stops at the sinks; see the class comment.
  if (on_worker_thread() || !has_subscribers_.load(std::memory_order_acquire)) return;
  {
    boost::lock_guard<boost::mutex> lock(delivery_->mutex);
    if (delivery_->stopping || delivery_->subscribers.empty()) return;
    // Full: drop the oldest line, counted as done so a flush still ends.
    // Reported when it starts, and with a count once the handlers catch up
    // (see deliver), not once per line - this is the logger's own channel.
    if (queue_limit_ > 0 && delivery_->queue.size() >= queue_limit_) {
      delivery_->queue.pop_front();
      ++delivery_->done;
      if (delivery_->dropped++ == 0) {
        logger_helper::log_fatal("Log handlers are " + std::to_string(delivery_->queue.size() + 1) +
                                 " lines behind; dropping the oldest lines for them until they catch up (the console and the log file are not affected)");
      }
    }
    delivery_->queue.push_back(data);
    ++delivery_->queued;
  }
  delivery_->changed.notify_all();
}

void nsclient_logger::deliver(const std::shared_ptr<delivery> d) {
  on_worker_thread() = true;
  boost::unique_lock<boost::mutex> lock(d->mutex);
  while (true) {
    while (!d->stopping && d->queue.empty()) d->changed.wait(lock);
    if (d->stopping) return;
    const std::string line = std::move(d->queue.front());
    d->queue.pop_front();
    if (d->dropped != 0 && d->queue.empty()) {
      logger_helper::log_fatal("Log handlers caught up; " + std::to_string(d->dropped) + " lines were dropped for them");
      d->dropped = 0;
    }
    // Walk by id rather than by position, so a subscriber added or removed
    // while the lock is down neither shifts the walk nor gets the line twice.
    std::uint64_t last = 0;
    while (!d->stopping) {
      const auto next = std::find_if(d->subscribers.begin(), d->subscribers.end(), [last](const entry &e) { return e.id > last; });
      if (next == d->subscribers.end()) break;
      last = next->id;
      if (!next->open) continue;
      d->current = next->subscriber;
      logging_subscriber_instance subscriber = next->subscriber;
      lock.unlock();
      try {
        subscriber->on_log_message(line);
      } catch (const std::exception &e) {
        logger_helper::log_fatal(std::string("Log handler failed: ") + e.what());
      } catch (...) {
        logger_helper::log_fatal("Log handler failed");
      }
      // Dropped before the remover can wake, so the remover, not this
      // thread, holds the last reference: a module is never destroyed (and
      // its library unmapped) on the logging thread.
      subscriber.reset();
      lock.lock();
      d->current.reset();
      d->changed.notify_all();
    }
    ++d->done;
    d->changed.notify_all();
  }
}

nsclient_logger::entries::iterator nsclient_logger::find(const logging_subscriber_instance &subscriber) {
  return std::find_if(delivery_->subscribers.begin(), delivery_->subscribers.end(), [&subscriber](const entry &e) { return e.subscriber == subscriber; });
}

// The wait excludes the worker itself - a handler closing itself - and is
// bounded like dll_plugin's wait for its dispatchers.
bool nsclient_logger::wait_until_left(boost::unique_lock<boost::mutex> &lock, const logging_subscriber_instance &subscriber) {
  if (on_worker_thread()) return true;
  return delivery_->changed.timed_wait(lock, boost::posix_time::milliseconds(delivery_wait_.count()),
                                       [&]() { return delivery_->current != subscriber; });
}

void nsclient_logger::add_subscriber(const logging_subscriber_instance subscriber) {
  boost::lock_guard<boost::mutex> lock(delivery_->mutex);
  if (delivery_->stopping) return;
  const entries::iterator it = find(subscriber);
  if (it != delivery_->subscribers.end()) {
    it->open = true;
  } else {
    delivery_->subscribers.push_back(entry{delivery_->next_id++, subscriber, true});
  }
  has_subscribers_ = true;
  if (!delivery_->started) {
    delivery_->started = true;
    const std::shared_ptr<delivery> d = delivery_;
    // This thread *is* the logger, so it reports its own death through the
    // last-resort channel.
    worker_ = threads::start_guarded_thread(
        "logger", [d]() { deliver(d); }, [](const std::string &message) { logger_helper::log_fatal(message); });
  }
}

// Close one subscriber in its place and wait until the worker is not inside
// it, so the caller can tear it down afterwards. Only that subscriber is
// waited for, and not at all when it was not on the list (the plugin manager
// closes every module it unloads, handler or not). One the worker is still
// inside stays closed where it is, so the next close waits for it again.
unsubscribe_result nsclient_logger::close_subscriber(const logging_subscriber_instance subscriber) {
  unsubscribe_result result;
  boost::unique_lock<boost::mutex> lock(delivery_->mutex);
  const entries::iterator it = find(subscriber);
  if (it == delivery_->subscribers.end()) return result;
  it->open = false;
  result.removed = true;
  result.delivering = !wait_until_left(lock, subscriber);
  return result;
}

void nsclient_logger::reopen_subscriber(const logging_subscriber_instance subscriber) {
  boost::lock_guard<boost::mutex> lock(delivery_->mutex);
  const entries::iterator it = find(subscriber);
  if (it != delivery_->subscribers.end()) it->open = true;
}

// The list's reference goes under the lock, but the caller still holds its
// own, so the module is never released here.
bool nsclient_logger::drop_subscriber(const logging_subscriber_instance subscriber) {
  boost::lock_guard<boost::mutex> lock(delivery_->mutex);
  const entries::iterator it = find(subscriber);
  if (it == delivery_->subscribers.end()) return false;
  // A handler dropping itself is the one line the worker can be inside here.
  if (delivery_->current == subscriber && !on_worker_thread()) return false;
  delivery_->subscribers.erase(it);
  has_subscribers_ = !delivery_->subscribers.empty();
  return true;
}

unsubscribe_result nsclient_logger::remove_subscriber(const logging_subscriber_instance subscriber) {
  const unsubscribe_result result = close_subscriber(subscriber);
  if (result.removed && !result.delivering) drop_subscriber(subscriber);
  return result;
}

// Take every subscriber off the list and wait for the worker to leave the
// one it is in, if any. Returns that one when the wait runs out, so the
// caller can leave its module alone.
std::vector<logging_subscriber_instance> nsclient_logger::clear_subscribers() {
  std::vector<logging_subscriber_instance> still_delivering;
  // Held until after the wait and released here, on the caller's thread, so
  // the worker never drops the last reference to a module (see deliver).
  entries taken;
  {
    boost::unique_lock<boost::mutex> lock(delivery_->mutex);
    // One deadline for both waits: first hand out the lines already queued,
    // so the last lines before the modules stop reach their handlers, then
    // take the list and wait for the worker to leave the one it is in.
    const boost::system_time deadline = boost::get_system_time() + boost::posix_time::milliseconds(delivery_wait_.count());
    wait_for_queued(lock, deadline);
    taken.swap(delivery_->subscribers);
    drop_queued();
    has_subscribers_ = false;
    if (on_worker_thread()) return still_delivering;
    if (!delivery_->changed.timed_wait(lock, deadline, [&]() { return !delivery_->current; })) {
      still_delivering.push_back(delivery_->current);
    }
  }
  return still_delivering;
}

bool nsclient_logger::startup() { return true; }

// Stop the worker. One stuck in a handler past the wait is left behind on the
// state it shares, and returns to nothing but that state.
bool nsclient_logger::shutdown() {
  entries taken;
  {
    boost::lock_guard<boost::mutex> lock(delivery_->mutex);
    delivery_->stopping = true;
    drop_queued();
    taken.swap(delivery_->subscribers);
  }
  delivery_->changed.notify_all();
  has_subscribers_ = false;
  if (!worker_) return true;
  const std::shared_ptr<boost::thread> worker = std::move(worker_);
  if (worker->get_id() == boost::this_thread::get_id()) {
    worker->detach();
    return true;
  }
  if (!worker->timed_join(boost::posix_time::milliseconds(join_wait_.count()))) {
    logger_helper::log_fatal("Log handler thread did not stop; leaving it behind");
    worker->detach();
    return false;
  }
  return true;
}

void nsclient_logger::destroy() {
  shutdown();
  boost::lock_guard<boost::mutex> lock(sink_mutex_);
  file_.reset();
}

void nsclient_logger::configure() {
  // Settings are read without the sink lock held: reading them may log.
  bool has_file;
  {
    boost::lock_guard<boost::mutex> lock(sink_mutex_);
    has_file = file_ != nullptr;
  }
  if (has_file) {
    const simple_file_logger::config_data config = simple_file_logger::do_config(true);
    boost::lock_guard<boost::mutex> lock(sink_mutex_);
    if (file_) file_->apply(config);
    return;
  }
  // Without a file only the date format is registered, as the console
  // backend did; the console renders without a date.
  try {
    std::string format;
    sh::settings_registry settings(settings_manager::get_proxy());
    settings.set_alias("log");
    settings.add_path_to_settings()("log", "Log file", "Configure log file properties.");
    settings.add_key_to_settings("log").add_string("date format", sh::string_key(&format, "%Y-%m-%d %H:%M:%S"), "Console date mask",
                                                   "The syntax of the dates in the log file.");
    settings.register_all();
    settings.notify();
  } catch (const std::exception &e) {
    logger_helper::log_fatal(std::string("Failed to configure logger: ") + e.what());
  } catch (...) {
    logger_helper::log_fatal("Failed to configure logger.");
  }
}

}  // namespace impl
}  // namespace logging
}  // namespace nsclient
