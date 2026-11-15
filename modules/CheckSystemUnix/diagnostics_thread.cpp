// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "diagnostics_thread.hpp"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <stdexcept>
#include <str/utf8.hpp>
#include <threads/guarded_thread.hpp>

namespace systemd_diagnostics {

namespace {
std::int64_t now() { return static_cast<std::int64_t>(std::time(nullptr)); }

// How many unit names one `systemctl show` is given (as service_source does).
const std::size_t show_batch = 128;
}  // namespace

bool collector::start() {
  if (!any()) return true;
  std::string error;
  if (!stop_signal_.create(error)) {
    // Without it a stop waits out the current sleep; the thread still runs.
    NSC_LOG_ERROR("Failed to create the diagnostics reader's stop signal, stopping it may take up to " + std::to_string(journal_seconds) + "s: " + error);
  }
  thread_ = threads::start_guarded_thread("checksystem diagnostics", [this]() { this->thread_proc(); }, NSC_THREAD_REPORTER);
  return true;
}

void collector::stop() {
  stop_requested_ = true;
  stop_signal_.signal();
  if (thread_) {
    thread_->join();
    thread_.reset();
  }
  stop_signal_.close();
}

void collector::add_sections(PB::Diagnostics::DiagnosticsMessage *out) const {
  if (!systemd_) return;
  service_tables::sample problems;
  event_tables::reader_state events_state, errors_state;
  {
    const std::lock_guard<std::mutex> guard(mutex_);
    problems = problems_;
    events_state = service_events_state_;
    errors_state = errors_state_;
  }
  if (services_) {
    service_tables::add_section(out, problems);
    // Nothing before the first read.
    if (events_state.captured_at != 0 || !events_state.error.empty()) event_tables::add_service_events(out, service_events_.copy(), events_state);
  }
  if (errors_ && (errors_state.captured_at != 0 || !errors_state.error.empty())) event_tables::add_errors(out, errors_ring_.copy(), errors_state);
}

void collector::refresh_problems() {
  service_tables::sample fresh;
  try {
    std::vector<std::string> candidates;
    for (const unit_listing &u : parse_list_units(source::systemctl({"list-units", "--type=service", "--all", "--no-legend", "--plain", "--no-pager"}))) {
      if (is_candidate(u)) candidates.push_back(u.id);
    }
    std::vector<service_tables::service_row> rows;
    const std::int64_t wall = now();
    const std::int64_t mono = source::monotonic_now_us();
    for (std::size_t begin = 0; begin < candidates.size(); begin += show_batch) {
      const std::size_t end = (std::min)(begin + show_batch, candidates.size());
      std::vector<std::string> args = {"show", "--no-pager", "--property=" + show_properties(), "--"};
      args.insert(args.end(), candidates.begin() + static_cast<std::ptrdiff_t>(begin), candidates.begin() + static_cast<std::ptrdiff_t>(end));
      for (const auto &unit : parse_show(source::systemctl(args))) {
        service_tables::service_row row;
        if (problem_row(unit, wall, mono, row)) rows.push_back(row);
      }
    }
    fresh = service_tables::make_sample(rows, wall);
  } catch (const std::exception &e) {
    fresh = service_tables::failed_sample(utf8::make_valid(e.what()), now());
  }
  const std::lock_guard<std::mutex> guard(mutex_);
  if (!fresh.error.empty() && fresh.error != problems_.error) NSC_LOG_ERROR("Failed to read the services for services.problems: " + fresh.error);
  problems_ = std::move(fresh);
}

void collector::read_journal(const feed which) {
  const bool service = which == feed::service_events;
  std::string &cursor = service ? service_events_cursor_ : errors_cursor_;
  std::int64_t &since = service ? service_events_since_ : errors_since_;
  event_tables::ring &ring = service ? service_events_ : errors_ring_;
  event_tables::reader_state &state = service ? service_events_state_ : errors_state_;

  const std::int64_t started = now();
  // The first read goes back seed_seconds; after a read that found nothing,
  // the next starts a second before it (the ring holds an entry once).
  if (since == 0) since = started - event_tables::seed_seconds;
  std::string error;
  try {
    const std::size_t limit = nscapi::diagnostics::max_rows;
    const journal_read read = parse_journal(which, source::journalctl(journal_args(which, cursor, since, limit)));
    if (!read.cursor.empty()) cursor = read.cursor;
    since = started - 1;
    // A read that came back full may have had more than it was allowed to
    // return: the ring then holds "at least" what it shows.
    if (read.rows.size() >= limit && !cursor.empty()) ring.count_dropped(1);
    for (const event_tables::event_row &row : read.rows) ring.push(row);
  } catch (const std::exception &e) {
    error = utf8::make_valid(e.what());
  }
  const std::lock_guard<std::mutex> guard(mutex_);
  if (!error.empty() && error != state.error)
    NSC_LOG_ERROR(std::string("Failed to read the journal for ") + (service ? event_tables::section_service_events : event_tables::section_errors) + ": " +
                  error);
  state.error = error;
  if (error.empty()) state.captured_at = started;
}

void collector::thread_proc() {
  if (!source::has_systemd()) {
    NSC_LOG_MESSAGE("No systemd on this host: the services and journal diagnostics tables are not produced");
    return;
  }
  systemd_ = true;

  using clock = std::chrono::steady_clock;
  const clock::duration problems_every = std::chrono::seconds(service_tables::refresh_seconds);
  const clock::duration journal_every = std::chrono::seconds(journal_seconds);
  clock::time_point problems_due = clock::now();
  clock::time_point journal_due = clock::now();

  while (!stop_requested_) {
    const clock::time_point t = clock::now();
    if (services_ && t >= problems_due) {
      refresh_problems();
      problems_due = clock::now() + problems_every;
    }
    if (stop_requested_) break;
    if (t >= journal_due) {
      if (services_) read_journal(feed::service_events);
      if (errors_ && !stop_requested_) read_journal(feed::errors);
      journal_due = clock::now() + journal_every;
    }
    clock::time_point next = journal_due;
    if (services_) next = (std::min)(next, problems_due);
    const auto wait = std::chrono::duration_cast<std::chrono::milliseconds>(next - clock::now());
    if (wait.count() <= 0) continue;
    const threads::stop_signal::wait_result waited = stop_signal_.wait_for(wait);
    if (waited == threads::stop_signal::wait_result::signalled) break;
    if (waited == threads::stop_signal::wait_result::failed) {
      // No usable stop signal: sleep in short steps so a stop is noticed.
      boost::this_thread::sleep_for(boost::chrono::milliseconds((std::min)(static_cast<long long>(wait.count()), 1000LL)));
    }
  }
}

}  // namespace systemd_diagnostics
