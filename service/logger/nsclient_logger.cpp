// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "nsclient_logger.hpp"

#include <algorithm>
#include <nsclient/logger/logger.hpp>

#include "simple_console_logger.hpp"
#include "simple_file_logger.hpp"
#include "threaded_logger.hpp"

#define CONSOLE_BACKEND "console"
#define THREADED_FILE_BACKEND "threaded-file"
#define FILE_BACKEND "file"
// Console everywhere, on every platform. Writing to a log file is a thing the
// *service* does - it selects the file backend explicitly when it starts (see
// cli_parser::parse_service, and the --log-backend in the systemd unit).
//
// It used to default to the file backend on Windows, which meant every CLI
// invocation opened a file next to the executable: under Program Files, so an
// ordinary user running `nscp client ...` failed to open it and logging quietly
// degraded. Nothing about a one-shot command needs a log file, and needing
// write access to the install directory to run one is worse than useless.
#define DEFAULT_BACKEND CONSOLE_BACKEND

void nsclient::logging::impl::nsclient_logger::set_backend(const std::string backend) {
  log_driver_instance tmp;
  if (backend == CONSOLE_BACKEND) {
    tmp = std::make_shared<simple_console_logger>(this);
  } else if (backend == THREADED_FILE_BACKEND) {
    log_driver_instance inner = std::make_shared<simple_file_logger>("nsclient.log");
    tmp = std::make_shared<threaded_logger>(this, inner);
  } else if (backend == FILE_BACKEND) {
    tmp = std::make_shared<simple_file_logger>("nsclient.log");
  } else {
    tmp = std::make_shared<simple_console_logger>(this);
  }
  use_backend(tmp);
}

void nsclient::logging::impl::nsclient_logger::use_backend(log_driver_instance backend) {
  if (!backend) return;
  if (backend_) backend->set_config(backend_);
  backend->startup();
  backend_.swap(backend);
}

nsclient::logging::impl::nsclient_logger::nsclient_logger() { nsclient_logger::set_backend(DEFAULT_BACKEND); }

nsclient::logging::impl::nsclient_logger::~nsclient_logger() { nsclient_logger::destroy(); }

void nsclient::logging::impl::nsclient_logger::destroy() { backend_.reset(); }

void nsclient::logging::impl::nsclient_logger::add_subscriber(const logging_subscriber_instance subscriber) {
  boost::lock_guard<boost::mutex> lock(mutex_);
  // Already here, closed by an earlier attempt: back in service in its
  // place, rather than a second gate the next removal would not know about.
  if (const entry *existing = find_locked(subscriber)) {
    existing->gate->tracker().reopen();
    return;
  }
  std::shared_ptr<subscribers_type> next = subscribers_ ? std::make_shared<subscribers_type>(*subscribers_) : std::make_shared<subscribers_type>();
  next->push_back(entry{next_id_++, std::make_shared<gate_type>(subscriber)});
  subscribers_ = next;
  has_subscribers_ = true;
}

const nsclient::logging::impl::nsclient_logger::entry *nsclient::logging::impl::nsclient_logger::find_locked(
    const logging_subscriber_instance &subscriber) const {
  if (!subscribers_) return nullptr;
  for (const entry &e : *subscribers_) {
    if (e.gate->value() == subscriber) return &e;
  }
  return nullptr;
}

// Close the subscriber's gate in place and wait for the lines inside it. A
// subscriber that is not on the list is not waited for - the plugin manager
// closes every module it unloads, handler or not - and a closed one that is
// closed again waits for the line that kept it closed: close() hands out a
// cutoff that still covers it.
nsclient::logging::unsubscribe_result nsclient::logging::impl::nsclient_logger::close_subscriber(logging_subscriber_instance subscriber) {
  unsubscribe_result result;
  std::shared_ptr<gate_type> gate;
  std::uint64_t cutoff = 0;
  {
    boost::lock_guard<boost::mutex> lock(mutex_);
    const entry *found = find_locked(subscriber);
    if (found == nullptr) return result;
    gate = found->gate;
    cutoff = gate->tracker().close();
  }
  result.removed = true;
  // The wait excludes this thread's own delivery - a handler closing itself
  // - and is bounded like dll_plugin's wait for its dispatchers: a handler
  // that has been running for five seconds is not going to finish because
  // we keep waiting.
  result.delivering = !gate->tracker().wait_for_others_before(cutoff, delivery_wait_);
  return result;
}

void nsclient::logging::impl::nsclient_logger::reopen_subscriber(logging_subscriber_instance subscriber) {
  boost::lock_guard<boost::mutex> lock(mutex_);
  if (const entry *found = find_locked(subscriber)) found->gate->tracker().reopen();
}

// Take a closed subscriber out of the list once nothing is inside it, and
// hand it back so this thread drops it - after mutex_ is released: when it
// is the last reference to a plugin, the destructor unmaps the library,
// whose static destructors may log, and a log line takes mutex_. One still
// occupied stays, closed, for the next close or clear to wait for.
bool nsclient::logging::impl::nsclient_logger::drop_subscriber(logging_subscriber_instance subscriber) {
  logging_subscriber_instance released;
  {
    boost::lock_guard<boost::mutex> lock(mutex_);
    const entry *found = find_locked(subscriber);
    if (found == nullptr) return false;
    threads::in_flight &tracker = found->gate->tracker();
    // A zero wait is a check: true once nothing that was inside is left.
    // The cutoff of a closed tracker covers every entry it will ever have.
    if (!tracker.wait_for_others_before(tracker.close(), std::chrono::milliseconds(0))) return false;
    const std::shared_ptr<gate_type> gate = found->gate;
    std::shared_ptr<subscribers_type> next = std::make_shared<subscribers_type>();
    next->reserve(subscribers_->size() - 1);
    for (const entry &e : *subscribers_) {
      if (e.gate != gate) next->push_back(e);
    }
    subscribers_ = next;
    has_subscribers_ = !next->empty();
    released = gate->take();
  }
  return true;
}

nsclient::logging::unsubscribe_result nsclient::logging::impl::nsclient_logger::remove_subscriber(logging_subscriber_instance subscriber) {
  const unsubscribe_result result = close_subscriber(subscriber);
  if (result.removed && !result.delivering) drop_subscriber(subscriber);
  return result;
}

// Close every subscriber and wait for the lines inside them within one
// shared bound, then drop the ones that came clear. The ones a line is
// still inside stay, closed, and are returned, so the caller can leave
// those modules alone - including one an earlier attempt left closed.
std::vector<nsclient::logging::logging_subscriber_instance> nsclient::logging::impl::nsclient_logger::clear_subscribers() {
  std::vector<std::pair<std::shared_ptr<gate_type>, std::uint64_t> > closing;
  {
    boost::lock_guard<boost::mutex> lock(mutex_);
    if (!subscribers_) return std::vector<logging_subscriber_instance>();
    for (const entry &e : *subscribers_) closing.emplace_back(e.gate, e.gate->tracker().close());
  }
  const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + delivery_wait_;
  std::vector<logging_subscriber_instance> still_delivering;
  std::vector<std::shared_ptr<gate_type> > clear;
  for (const auto &c : closing) {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const std::chrono::milliseconds remaining =
        now < deadline ? std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now) : std::chrono::milliseconds(0);
    if (c.first->tracker().wait_for_others_before(c.second, remaining)) {
      clear.push_back(c.first);
    } else {
      still_delivering.push_back(c.first->value());
    }
  }
  std::vector<logging_subscriber_instance> released;
  {
    boost::lock_guard<boost::mutex> lock(mutex_);
    std::shared_ptr<subscribers_type> next = std::make_shared<subscribers_type>();
    for (const entry &e : *subscribers_) {
      if (std::find(clear.begin(), clear.end(), e.gate) == clear.end()) {
        next->push_back(e);
      } else {
        released.push_back(e.gate->take());
      }
    }
    subscribers_ = next;
    has_subscribers_ = !next->empty();
  }
  return still_delivering;
}

void nsclient::logging::impl::nsclient_logger::on_log_message(const std::string &data) { deliver(data, log_handler_chain()); }

void nsclient::logging::impl::nsclient_logger::on_handler_log_message(const std::string &data, const log_handler_chain &chain) { deliver(data, chain); }

void nsclient::logging::impl::nsclient_logger::deliver(const std::string &data, const log_handler_chain &chain) {
  if (!has_subscribers_.load(std::memory_order_acquire)) return;
  subscribers_ptr snapshot;
  {
    boost::lock_guard<boost::mutex> lock(mutex_);
    snapshot = subscribers_;
  }
  if (!snapshot) return;
  delivery_context context{&chain, 0};
  struct restore_context {
    delivery_context *outer;
    ~restore_context() { current_delivery() = outer; }
  } restore{current_delivery()};
  current_delivery() = &context;
  for (const entry &e : *snapshot) {
    if (std::find(chain.begin(), chain.end(), e.id) != chain.end()) continue;
    // The guard is declared before the copy, so the copy goes first however
    // this ends: the gate still holds the subscriber, so the copy is never
    // the last reference, and only then does a waiting close wake.
    threads::in_flight::guard inside(e.gate->tracker());
    if (!inside.try_enter()) continue;  // closed since the list was read
    logging_subscriber_instance subscriber = e.gate->value();
    context.handler = e.id;
    subscriber->on_log_message(data);
    context.handler = 0;
    subscriber.reset();
    inside.leave();
  }
}

bool nsclient::logging::impl::nsclient_logger::startup() {
  if (backend_) {
    return backend_->startup();
  } else {
    return false;
  }
}
bool nsclient::logging::impl::nsclient_logger::shutdown() {
  if (backend_) {
    return backend_->shutdown();
  }
  return false;
}
void nsclient::logging::impl::nsclient_logger::configure() {
  if (backend_) {
    backend_->synch_configure();
    backend_->asynch_configure();
  }
}

void nsclient::logging::impl::nsclient_logger::do_log(const std::string data) {
  if (!backend_) return;
  const delivery_context *const context = current_delivery();
  if (context != nullptr && context->handler != 0) {
    log_handler_chain chain = *context->chain;
    chain.push_back(context->handler);
    backend_->do_log_from_handler(data, chain);
  } else {
    backend_->do_log(data);
  }
}
