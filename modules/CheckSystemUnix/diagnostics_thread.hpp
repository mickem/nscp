// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/thread/thread.hpp>
#include <cstdint>
#include <diagnostics/event_tables.hpp>
#include <diagnostics/service_tables.hpp>
#include <memory>
#include <mutex>
#include <nscapi/nscapi_diagnostics_helper.hpp>
#include <string>
#include <threads/stop_signal.hpp>
#include <vector>

#include "diagnostics_systemd.h"

namespace systemd_diagnostics {

// The platform reader, diagnostics_source_<platform>.cpp.
namespace source {
// Whether the host runs systemd (/run/systemd/system). Without it there are
// no sections at all.
bool has_systemd();
// Run systemctl or journalctl with `args`, bounded in time and output, and
// return what it wrote. Throws std::runtime_error when it could not be run,
// timed out or failed.
std::string systemctl(const std::vector<std::string> &args);
std::string journalctl(const std::vector<std::string> &args);
// CLOCK_MONOTONIC in microseconds, the clock systemd's *Monotonic properties
// are on.
std::int64_t monotonic_now_us();
}  // namespace source

// The systemd diagnostics sections ([/settings/system/unix/diagnostics]):
//
//   services = true   services.problems (systemctl, every refresh_seconds) and
//                     services.events (the manager's unit failure and restart
//                     messages in the journal)
//   errors = true     eventlog.errors (journal entries at priority err or worse)
//
// One thread of its own runs the tools - never fetchDiagnostics, and never the
// 1 Hz collector, which a slow systemctl must not hold up. The journal is read
// every journal_seconds from where the previous read stopped (its cursor); the
// first read goes back event_tables::seed_seconds so a failure soon after the
// agent started has its run-up. fetchDiagnostics copies what the thread holds.
class collector {
 public:
  static constexpr int journal_seconds = 10;

  collector(bool services, bool errors) : services_(services), errors_(errors) {}
  ~collector() { stop(); }
  collector(const collector &) = delete;
  collector &operator=(const collector &) = delete;

  bool any() const { return services_ || errors_; }
  bool start();
  void stop();

  // Cheap: copies what the thread last read. Nothing for a section that is
  // off, and nothing at all on a host without systemd.
  void add_sections(PB::Diagnostics::DiagnosticsMessage *out) const;

 private:
  void thread_proc();
  void refresh_problems();
  void read_journal(feed which);

  const bool services_;
  const bool errors_;
  // Set once the thread has looked: until then, and on a host without
  // systemd, there are no sections.
  std::atomic<bool> systemd_{false};

  event_tables::ring service_events_;
  event_tables::ring errors_ring_;
  mutable std::mutex mutex_;
  service_tables::sample problems_;
  event_tables::reader_state service_events_state_;
  event_tables::reader_state errors_state_;

  // Touched only by the thread.
  std::string service_events_cursor_;
  std::string errors_cursor_;
  std::int64_t service_events_since_ = 0;
  std::int64_t errors_since_ = 0;
  // Every running unit is read on a sweep, one refresh in sweep_every (five
  // minutes at the default refresh); the refreshes in between read the
  // troubled units and the running ones the last sweep found restarted.
  static constexpr int sweep_every = 10;
  int refreshes_until_sweep_ = 0;
  std::set<std::string> restarted_running_;

  std::shared_ptr<boost::thread> thread_;
  threads::stop_signal stop_signal_;
  std::atomic<bool> stop_requested_{false};
};

}  // namespace systemd_diagnostics
