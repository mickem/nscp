// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "growth_spots.hpp"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <sstream>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <str/utf8.hpp>
#include <str/utils_no_boost.hpp>
#include <str/xtos.hpp>
#include <threads/guarded_thread.hpp>

#include "largest_files_scan.hpp"

namespace growth_spots {

namespace {
std::string comparable(std::string path, const bool ignore_case) {
  while (path.size() > 1 && (path.back() == '/' || path.back() == '\\')) path.pop_back();
  // ASCII only: enough for drive letters and the system's own folder names,
  // and it never mangles the UTF-8 of anything else.
  return ignore_case ? str::utils::ascii_lower(path) : path;
}

std::string plural(const unsigned long long n, const std::string &one, const std::string &many) { return str::xtos(n) + " " + (n == 1 ? one : many); }

// System messages end in a line break (FormatMessage, strerror on some libcs).
std::string trimmed(std::string text) {
  while (!text.empty() && (text.back() == '\n' || text.back() == '\r' || text.back() == ' ' || text.back() == '\t')) text.pop_back();
  return text;
}
}  // namespace

std::vector<spot> dedupe(const std::vector<spot> &spots, const bool ignore_case) {
  std::vector<spot> out;
  std::vector<std::string> seen;
  for (const spot &s : spots) {
    if (s.path.empty()) continue;
    const std::string key = comparable(s.path, ignore_case);
    if (std::find(seen.begin(), seen.end(), key) != seen.end()) continue;
    seen.push_back(key);
    out.push_back(s);
  }
  return out;
}

std::string describe(const dir_size &d) {
  std::string note;
  if (d.timed_out) note = "time limit reached: the size counted so far";
  if (d.unreadable > 0) {
    if (!note.empty()) note += "; ";
    note += plural(d.unreadable, "folder or entry", "folders or entries") + " could not be read";
  }
  return note;
}

sample measure(const std::vector<spot> &spots, const limits &lim, const hooks &h) {
  sample out;
  // Walking time only: the pauses between directories are the point of the
  // pauses, and must not eat into what the walks themselves may take.
  walk_clock::duration spent = walk_clock::duration::zero();
  bool first_walk = true;
  for (const spot &s : spots) {
    row r;
    r.path = s.path;
    r.kind = s.kind;
    if (s.type == spot::measured) {
      r.has_size = s.has_size;
      r.size = s.size;
      r.note = trimmed(s.note);
      out.rows.push_back(r);
      continue;
    }
    if (s.type == spot::file) {
      unsigned long long size = 0;
      std::string error;
      if (!h.file_size(s.path, size, error)) continue;
      if (error.empty()) {
        r.has_size = true;
        r.size = static_cast<long long>((std::min)(size, static_cast<unsigned long long>(INT64_MAX)));
      } else {
        r.note = trimmed(error);
      }
      out.rows.push_back(r);
      continue;
    }
    // A directory: one at a time, with a pause before each but the first.
    if (!first_walk && h.pause && !h.pause()) return out;
    first_walk = false;
    const walk_clock::duration left = lim.overall - spent;
    if (left <= walk_clock::duration::zero()) {
      r.note = "not measured: the round ran out of time";
      out.truncated = true;
      out.rows.push_back(r);
      continue;
    }
    const walk_clock::duration budget = (std::min)(walk_clock::duration(lim.per_directory), left);
    const walk_clock::time_point started = h.now();
    const dir_size d = h.walk_directory(s.path, started + budget);
    spent += h.now() - started;
    if (d.missing) continue;
    if (!d.error.empty() && d.files == 0 && d.bytes == 0) {
      r.note = trimmed(d.error);
      out.rows.push_back(r);
      continue;
    }
    r.has_size = true;
    r.size = static_cast<long long>((std::min)(d.bytes, static_cast<unsigned long long>(INT64_MAX)));
    r.note = describe(d);
    if (d.timed_out) out.truncated = true;
    out.rows.push_back(r);
  }
  return out;
}

// /proc/swaps escapes blanks in a path as octal (`\040`); undo that.
std::string unescape_proc_path(const std::string &in) {
  std::string out;
  for (std::size_t i = 0; i < in.size(); ++i) {
    if (in[i] == '\\' && i + 3 < in.size() && in[i + 1] >= '0' && in[i + 1] <= '3' && in[i + 2] >= '0' && in[i + 2] <= '7' && in[i + 3] >= '0' &&
        in[i + 3] <= '7') {
      out += static_cast<char>((in[i + 1] - '0') * 64 + (in[i + 2] - '0') * 8 + (in[i + 3] - '0'));
      i += 3;
      continue;
    }
    out += in[i];
  }
  return out;
}

// The swap files in a /proc/swaps listing (header line first).
std::vector<std::string> parse_swap_files(std::istream &in) {
  std::vector<std::string> out;
  std::string line;
  std::getline(in, line);  // Filename Type Size Used Priority
  while (std::getline(in, line)) {
    std::istringstream fields(line);
    std::string name, type;
    if (!(fields >> name >> type)) continue;
    if (type == "file") out.push_back(unescape_proc_path(name));
  }
  return out;
}

void add_section(PB::Diagnostics::DiagnosticsMessage *out, const sample &s) {
  using nscapi::diagnostics::type_int;
  using nscapi::diagnostics::type_string;
  if (s.captured_at == 0) return;
  auto t = nscapi::diagnostics::table(out, section_name)
               .describe("Size of the places that usually fill a disk: temp and log directories, page and swap files, shadow-copy storage")
               .captured_at(s.captured_at);
  t.column("path")
      .column("kind", type_string, "", "temp, logs, page_file, swap_file or shadow_storage")
      .column("size", type_int, "bytes", "The directory tree's files, the file, or the shadow storage in use")
      .column("note", type_string, "", "What limited the size (a time limit, unreadable entries), or why there is none; empty when complete");
  if (!s.error.empty()) {
    t.error(s.error);
    return;
  }
  if (s.truncated) t.truncated();
  for (const row &r : s.rows) {
    auto line = t.row();
    line.add(r.path).add(r.kind);
    if (r.has_size)
      line.add(r.size);
    else
      line.add_null();
    if (r.note.empty())
      line.add_null();
    else
      line.add(r.note);
  }
}

// --- the thread ---------------------------------------------------------------

void walker::start() {
  {
    const boost::lock_guard<boost::mutex> lock(stop_mutex_);
    stop_requested_ = false;
  }
  abort_ = false;
  thread_ = threads::start_guarded_thread("checkdisk growth spots", [this]() { this->thread_proc(); }, NSC_THREAD_REPORTER);
}

void walker::stop() {
  {
    const boost::lock_guard<boost::mutex> lock(stop_mutex_);
    stop_requested_ = true;
  }
  abort_ = true;
  stop_cv_.notify_all();
  if (thread_) {
    thread_->join();
    thread_.reset();
  }
}

sample walker::get() const {
  const boost::lock_guard<boost::mutex> lock(mutex_);
  return last_;
}

void walker::seed(const sample &s) {
  const boost::lock_guard<boost::mutex> lock(mutex_);
  last_ = s;
}

bool walker::wait(const std::chrono::milliseconds d) {
  boost::unique_lock<boost::mutex> lock(stop_mutex_);
  return !stop_cv_.timed_wait(lock, boost::posix_time::milliseconds(d.count()), [this]() { return stop_requested_; });
}

dir_size walk_directory(const std::string &path, const walk_clock::time_point deadline, const std::atomic<bool> *abort) {
  largest_files::scan_request request;
  request.root = path;
  request.top = 0;  // nothing ranked: only the totals
  request.method = largest_files::scan_method::walk;
  request.same_filesystem = true;
  request.deadline = deadline;
  request.abort = abort;
  const largest_files::scan_result result = largest_files::scan(request);
  dir_size out;
  if (result.error.compare(0, 14, "Path not found") == 0 || result.error.compare(0, 12, "Not a folder") == 0) {
    out.missing = true;
    return out;
  }
  out.bytes = result.bytes_seen;
  out.files = result.files_seen;
  out.timed_out = result.timed_out;
  out.error = result.error;
  // The root itself is counted in `error`, not again as an unreadable folder.
  out.unreadable = result.unreadable > 0 && !result.error.empty() ? result.unreadable - 1 : result.unreadable;
  return out;
}

std::chrono::seconds first_wait(const std::chrono::seconds first_delay, const std::chrono::seconds interval, const std::int64_t seeded_at,
                                const std::int64_t now) {
  if (seeded_at <= 0) return first_delay;
  const std::int64_t age = now > seeded_at ? now - seeded_at : 0;
  const std::chrono::seconds left = interval - std::chrono::seconds(age);
  return left > first_delay ? left : first_delay;
}

void walker::thread_proc() {
  lower_priority();
  if (!wait(first_wait(first_delay, interval, get().captured_at, static_cast<std::int64_t>(std::time(nullptr))))) return;
  for (;;) {
    sample fresh;
    try {
      hooks h;
      h.walk_directory = [this](const std::string &path, const walk_clock::time_point deadline) { return walk_directory(path, deadline, &abort_); };
      h.file_size = &growth_spots::file_size;
      // A breath between directories, so a struggling disk serves its real
      // work in between.
      h.pause = [this]() { return wait(std::chrono::milliseconds(500)); };
      h.now = []() { return walk_clock::now(); };
      fresh = measure(dedupe(discover(agent_log_dir), paths_ignore_case()), lim, h);
    } catch (const std::exception &e) {
      fresh = sample();
      fresh.error = "Failed to measure the growth spots: " + utf8::utf8_from_native(e.what());
    }
    // A round cut short by stop() is not a result.
    if (abort_.load()) return;
    fresh.captured_at = static_cast<std::int64_t>(std::time(nullptr));
    {
      const boost::lock_guard<boost::mutex> lock(mutex_);
      if (!fresh.error.empty() && fresh.error != last_.error) NSC_LOG_ERROR(fresh.error);
      last_ = std::move(fresh);
    }
    if (!wait(interval)) return;
  }
}

}  // namespace growth_spots
