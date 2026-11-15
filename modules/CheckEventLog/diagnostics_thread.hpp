// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/thread/thread.hpp>
#include <cstdint>
#include <diagnostics/event_tables.hpp>
#include <memory>
#include <mutex>
#include <nscapi/nscapi_diagnostics_helper.hpp>
#include <string>
#include <threads/stop_signal.hpp>

// The event log diagnostics sections ([/settings/eventlog/diagnostics]):
//
//   services = true   services.events: the service control manager's crash
//                     events (7031, 7034, 7024) from the System log
//   errors = true     eventlog.errors: every critical or error event written
//                     to the Application and System logs
//
// One thread holds its own event log subscriptions for them - through the
// same eventlog_wrapper_new the real-time filters use, but separate from
// them, so it works whether or not `real-time` is enabled and never touches
// those filters' logs or state. Each event is rendered once, when it arrives
// (the message is the expensive, localised part), and kept in a ring;
// fetchDiagnostics copies the rings.
//
// At start (and so after every settings reload) each ring is seeded with one
// backward query of the last event_tables::seed_seconds, so a failure soon
// after the agent started still has the events that led up to it.
class eventlog_diagnostics {
 public:
  eventlog_diagnostics(bool services, bool errors) : services_(services), errors_(errors) {}
  ~eventlog_diagnostics() { stop(); }
  eventlog_diagnostics(const eventlog_diagnostics &) = delete;
  eventlog_diagnostics &operator=(const eventlog_diagnostics &) = delete;

  bool any() const { return services_ || errors_; }
  bool start();
  void stop();

  // Cheap: copies the rings. Nothing for a section that is off.
  void add_sections(PB::Diagnostics::DiagnosticsMessage *out) const;

  // The XPath filters, `seed_seconds` > 0 limiting them to that many seconds
  // back for the backward query a ring is seeded with.
  static std::string service_events_query(std::int64_t seed_seconds = 0);
  static std::string errors_query(std::int64_t seed_seconds = 0);

 private:
  void thread_proc();
  void set_error(bool service, const std::string &error);

  const bool services_;
  const bool errors_;
  event_tables::ring service_events_;
  event_tables::ring errors_ring_;
  mutable std::mutex state_mutex_;
  std::string services_error_;
  std::string errors_error_;
  std::shared_ptr<boost::thread> thread_;
  threads::stop_signal stop_signal_;
};
