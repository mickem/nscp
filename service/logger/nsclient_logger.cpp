// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "nsclient_logger.hpp"

#include <algorithm>
#include <nscapi/protobuf/log.hpp>
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
  std::vector<logging_subscriber_instance> released;
  boost::lock_guard<boost::mutex> lock(mutex_);
  prune_draining(released);
  std::shared_ptr<subscribers_type> next = subscribers_ ? std::make_shared<subscribers_type>(*subscribers_) : std::make_shared<subscribers_type>();
  next->push_back(entry{next_id_++, std::make_shared<gate_type>(subscriber)});
  subscribers_ = next;
  has_subscribers_ = true;
}

void nsclient::logging::impl::nsclient_logger::prune_draining(std::vector<logging_subscriber_instance> &released) {
  for (std::vector<draining>::iterator it = draining_.begin(); it != draining_.end();) {
    // A zero wait is a check: true once nothing that was inside is left.
    if (it->subscriber.gate->tracker().wait_for_others_before(it->cutoff, std::chrono::milliseconds(0))) {
      released.push_back(it->subscriber.gate->take());
      it = draining_.erase(it);
    } else {
      ++it;
    }
  }
}

// Take one subscriber off the list, close its gate and wait for the lines
// still inside it, so the caller can tear it down afterwards. Only that
// subscriber's lines are waited for; a subscriber that was not on the list
// is not waited for at all - the plugin manager unsubscribes every module it
// unloads, handler or not - and costs no copy: the list is scanned first.
nsclient::logging::unsubscribe_result nsclient::logging::impl::nsclient_logger::remove_subscriber(logging_subscriber_instance subscriber) {
  unsubscribe_result result;
  // Released after every lock is dropped, on this thread: when one of these
  // is the last reference to a plugin, its destructor unmaps the library,
  // whose static destructors may log - and a log line takes mutex_.
  std::vector<logging_subscriber_instance> released;
  entry removed{0, nullptr};
  std::uint64_t cutoff = 0;
  {
    boost::lock_guard<boost::mutex> lock(mutex_);
    prune_draining(released);
    if (!subscribers_) return result;
    std::size_t at = subscribers_->size();
    for (std::size_t i = 0; i < subscribers_->size(); ++i) {
      if ((*subscribers_)[i].gate->value() == subscriber) {
        at = i;
        break;
      }
    }
    if (at == subscribers_->size()) return result;
    std::shared_ptr<subscribers_type> next = std::make_shared<subscribers_type>();
    next->reserve(subscribers_->size() - 1);
    for (std::size_t i = 0; i < subscribers_->size(); ++i) {
      if (i != at) next->push_back((*subscribers_)[i]);
    }
    removed = (*subscribers_)[at];
    cutoff = removed.gate->tracker().close();
    subscribers_ = next;
    has_subscribers_ = !next->empty();
  }
  result.removed = true;
  // The wait excludes this thread's own delivery - a handler unsubscribing
  // itself - and is bounded like dll_plugin's wait for its dispatchers: a
  // handler that has been running for five seconds is not going to finish
  // because we keep waiting.
  if (removed.gate->tracker().wait_for_others_before(cutoff, delivery_wait_)) {
    released.push_back(removed.gate->take());
  } else {
    result.delivering = true;
    boost::lock_guard<boost::mutex> lock(mutex_);
    draining_.push_back(draining{removed, cutoff});
  }
  return result;
}

// Take every subscriber off the list, together with the ones an earlier
// removal could not wait out, and wait for the lines still inside them
// within one shared bound. Returns the subscribers a line is still inside,
// so the caller can leave those modules alone; they stay draining.
std::vector<nsclient::logging::logging_subscriber_instance> nsclient::logging::impl::nsclient_logger::clear_subscribers() {
  std::vector<logging_subscriber_instance> released;
  std::vector<logging_subscriber_instance> still_delivering;
  std::vector<draining> closing;
  {
    boost::lock_guard<boost::mutex> lock(mutex_);
    if (subscribers_) {
      for (const entry &e : *subscribers_) closing.push_back(draining{e, e.gate->tracker().close()});
    }
    subscribers_ = std::make_shared<subscribers_type>();
    has_subscribers_ = false;
    closing.insert(closing.end(), draining_.begin(), draining_.end());
    draining_.clear();
  }
  const std::chrono::steady_clock::time_point deadline = std::chrono::steady_clock::now() + delivery_wait_;
  std::vector<draining> stuck;
  for (const draining &d : closing) {
    const std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
    const std::chrono::milliseconds remaining =
        now < deadline ? std::chrono::duration_cast<std::chrono::milliseconds>(deadline - now) : std::chrono::milliseconds(0);
    if (d.subscriber.gate->tracker().wait_for_others_before(d.cutoff, remaining)) {
      released.push_back(d.subscriber.gate->take());
    } else {
      still_delivering.push_back(d.subscriber.gate->value());
      stuck.push_back(d);
    }
  }
  if (!stuck.empty()) {
    boost::lock_guard<boost::mutex> lock(mutex_);
    draining_.insert(draining_.end(), stuck.begin(), stuck.end());
  }
  return still_delivering;
}

void nsclient::logging::impl::nsclient_logger::on_log_message(const std::string &data) {
  if (!has_subscribers_.load(std::memory_order_acquire)) return;
  const std::vector<std::uint64_t> chain = handler_chain(data);
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
    // the last reference, and only then does a waiting remove() wake.
    threads::in_flight::guard inside(e.gate->tracker());
    if (!inside.try_enter()) continue;  // closed for removal since the list was read
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
  // A line logged from inside a handler names that handler, and the chain
  // of the line it was handed, in the line itself - so it is not handed back
  // to any of them when it comes through on_log_message, synchronously on
  // this thread on the console backend or from the worker's queue on the
  // threaded one. It still goes to the backend, so it reaches the console or
  // file like any other, and to every handler not in its chain.
  const delivery_context *const context = current_delivery();
  if (context != nullptr && context->handler != 0) {
    std::vector<std::uint64_t> chain = *context->chain;
    chain.push_back(context->handler);
    backend_->do_log(tag_handler_line(data, chain));
  } else {
    backend_->do_log(data);
  }
}

namespace {
// LogEntry.handled_by: field 2, length-delimited (packed). An untagged line
// starts with field 1 (0x0A), the only other field, so the first byte tells
// the two apart without a parse.
const unsigned char handled_by_key = (2 << 3) | 2;

void append_varint(std::string &out, std::uint64_t value) {
  while (value >= 0x80) {
    out.push_back(static_cast<char>((value & 0x7F) | 0x80));
    value >>= 7;
  }
  out.push_back(static_cast<char>(value));
}
}  // namespace

std::string nsclient::logging::impl::nsclient_logger::tag_handler_line(const std::string &data, const std::vector<std::uint64_t> &chain) {
  PB::Log::LogEntry message;
  // A line that is not a LogEntry cannot be tagged; it goes out as it is and
  // is delivered as any other, which is what happened to it before.
  if (!message.ParseFromString(data)) return data;
  // Serialised without the field, which then goes in front by hand: a
  // serialiser writes fields in number order, which would put it after the
  // entries, where the first-byte check could not see it.
  message.clear_handled_by();
  std::string packed;
  for (const std::uint64_t id : chain) append_varint(packed, id);
  std::string out;
  out.push_back(static_cast<char>(handled_by_key));
  append_varint(out, packed.size());
  out += packed;
  out += message.SerializeAsString();
  return out;
}

std::vector<std::uint64_t> nsclient::logging::impl::nsclient_logger::handler_chain(const std::string &data) {
  std::vector<std::uint64_t> chain;
  if (data.empty() || static_cast<unsigned char>(data[0]) != handled_by_key) return chain;
  PB::Log::LogEntry message;
  if (!message.ParseFromString(data)) return chain;
  chain.assign(message.handled_by().begin(), message.handled_by().end());
  return chain;
}
