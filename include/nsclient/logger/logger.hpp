// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <string>
#include <vector>

#define LOG_CRITICAL_CORE(msg)                               \
  {                                                          \
    get_logger()->critical("core", __FILE__, __LINE__, msg); \
  }
#define LOG_CRITICAL_CORE_STD(msg) LOG_CRITICAL_CORE(std::string(msg))
#define LOG_ERROR_CORE(msg)                               \
  {                                                       \
    get_logger()->error("core", __FILE__, __LINE__, msg); \
  }
#define LOG_ERROR_CORE_STD(msg) LOG_ERROR_CORE(std::string(msg))
#define LOG_WARN_CORE(msg)                                  \
  {                                                         \
    get_logger()->warning("core", __FILE__, __LINE__, msg); \
  }
#define LOG_WARN_CORE_STD(msg) LOG_WARN_CORE(std::string(msg))
#define LOG_INFO_CORE(msg)                               \
  {                                                      \
    get_logger()->info("core", __FILE__, __LINE__, msg); \
  }
#define LOG_INFO_CORE_STD(msg) LOG_INFO_CORE(std::string(msg))
#define LOG_DEBUG_CORE(msg)                               \
  {                                                       \
    get_logger()->debug("core", __FILE__, __LINE__, msg); \
  }
#define LOG_DEBUG_CORE_STD(msg) LOG_DEBUG_CORE(std::string(msg))
#define LOG_DEBUG_CORE_RAW(file, line, msg)       \
  {                                               \
    get_logger()->debug("core", file, line, msg); \
  }
#define IS_LOG_DEBUG_CORE() \
  {                         \
    if (get_logger()->should_debug())
#define LOG_TRACE_CORE(msg)                               \
  {                                                       \
    get_logger()->trace("core", __FILE__, __LINE__, msg); \
  }
#define LOG_TRACE_CORE_STD(msg) LOG_DEBUG_CORE(std::string(msg))
#define IS_LOG_TRACE_CORE() if (get_logger()->should_trace())
#define IS_NOT_LOG_DEBUG_CORE() if (!get_logger()->should_debug())

namespace nsclient {
namespace logging {
struct logging_subscriber {
  virtual ~logging_subscriber() = default;
  virtual void on_log_message(const std::string &payload) = 0;
};
typedef std::shared_ptr<logging_subscriber> logging_subscriber_instance;

struct log_interface {
  virtual ~log_interface() = default;
  virtual void trace(const std::string &module, const char *file, const int line, const std::string &message) = 0;
  virtual void debug(const std::string &module, const char *file, const int line, const std::string &message) = 0;
  virtual void info(const std::string &module, const char *file, const int line, const std::string &message) = 0;
  virtual void warning(const std::string &module, const char *file, const int line, const std::string &message) = 0;
  virtual void error(const std::string &module, const char *file, const int line, const std::string &message) = 0;
  virtual void critical(const std::string &module, const char *file, const int line, const std::string &message) = 0;

  virtual bool should_trace() const = 0;
  virtual bool should_debug() const = 0;
  virtual bool should_info() const = 0;
  virtual bool should_warning() const = 0;
  virtual bool should_error() const = 0;
  virtual bool should_critical() const = 0;
};

// What close_subscriber / remove_subscriber found. `removed` says the
// subscriber was on the list; `delivering` says a log line is still inside
// it, because the bounded wait for the deliveries in flight ran out. A
// caller about to tear the subscriber down (unloading the module) treats
// `delivering` the way it treats a dispatch that will not finish: it
// refuses, and reopens the subscriber.
struct unsubscribe_result {
  bool removed = false;
  bool delivering = false;
};

struct logger : log_interface {
  virtual void raw(const std::string &message) = 0;

  virtual void add_subscriber(logging_subscriber_instance subscriber) = 0;
  // The two-phase removal an unload needs, the shape the walk lists have:
  // close the subscriber in its place and wait for the lines inside it;
  // then either reopen it there (the unload was refused) or drop it (the
  // module is gone). A subscriber a line is still inside stays closed in
  // its place, so the next close waits for that line again.
  virtual unsubscribe_result close_subscriber(logging_subscriber_instance subscriber) = 0;
  virtual void reopen_subscriber(logging_subscriber_instance subscriber) = 0;
  // Takes a subscriber out once nothing is inside it; false when one is.
  virtual bool drop_subscriber(logging_subscriber_instance subscriber) = 0;
  // close, then drop when nothing was inside.
  virtual unsubscribe_result remove_subscriber(logging_subscriber_instance subscriber) = 0;
  // Takes every subscriber off the list and waits for the deliveries in
  // flight. Returns the subscribers a line was still inside when the wait
  // ran out, so the caller can leave those alone; empty means all clear.
  virtual std::vector<logging_subscriber_instance> clear_subscribers() = 0;

  virtual bool startup() = 0;
  virtual bool shutdown() = 0;
  virtual void destroy() = 0;
  virtual void configure() = 0;

  virtual void set_log_level(std::string level) = 0;
  virtual std::string get_log_level() const = 0;

  virtual void set_backend(std::string backend) = 0;
};
typedef std::shared_ptr<logger> logger_instance;
typedef std::shared_ptr<log_interface> log_client_accessor;

}
}