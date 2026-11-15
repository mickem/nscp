// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The `disk.growth_spots` diagnostics section: how big the places that
// usually fill a disk are right now - the temp directories, the log
// directories, the page and swap files and (Windows) the shadow-copy storage.
// A failed check_drivesize is a symptom; this is where to look for the cause,
// next to the used_change_1h/24h metrics that say which volume is growing.
//
// Directory sizes are expensive, so they are never taken on demand. The
// module's own guarded thread (growth_spots::walker) measures the spots on a
// slow cadence - ten minutes by default, the first time shortly after the
// section is switched on - and fetchDiagnostics copies the last result, with
// `captured_at` set to when that walk finished.
//
// A walk is meant to be harmless on a host that is already struggling: one
// directory at a time with a pause between them, at background I/O priority,
// with a time limit per directory and for the whole round. A directory that
// hits its limit is reported with the size counted so far, the row says so in
// `note`, and the section is marked truncated. Links, junctions and other
// name-surrogate reparse points are not followed and a walk stays on the
// filesystem it started on; entries that cannot be read are skipped and
// counted in the note, so an unelevated agent still reports what it can see.
//
// Everything here is platform neutral and unit tested with a fake clock and
// fake sizers. What the spots are on this host, how big a single file is and
// how the thread lowers its priority are per platform (growth_spots_win.cpp,
// growth_spots_linux.cpp).

#include <atomic>
#include <boost/thread/condition_variable.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <cstdint>
#include <functional>
#include <istream>
#include <memory>
#include <nscapi/nscapi_diagnostics_helper.hpp>
#include <string>
#include <vector>

namespace growth_spots {

static constexpr const char *section_name = "disk.growth_spots";

// The `kind` column.
static constexpr const char *kind_temp = "temp";
static constexpr const char *kind_logs = "logs";
static constexpr const char *kind_page_file = "page_file";
static constexpr const char *kind_swap_file = "swap_file";
static constexpr const char *kind_shadow_storage = "shadow_storage";

typedef std::chrono::steady_clock walk_clock;

// One place to measure.
struct spot {
  enum type_t {
    // A directory tree, walked.
    directory,
    // A single file (a page or swap file), its size read.
    file,
    // Already measured by the platform (shadow storage): `size`/`note` as given.
    measured
  };
  std::string path;
  std::string kind;
  type_t type = directory;
  bool has_size = false;
  long long size = 0;
  std::string note;
};

// One row of the table.
struct row {
  std::string path;
  std::string kind;
  // Bytes; false when nothing could be measured (the row then says why).
  bool has_size = false;
  long long size = 0;
  // Empty when the size is complete; otherwise what limited it.
  std::string note;
};

struct sample {
  // When the walk finished; 0 until the first one has.
  std::int64_t captured_at = 0;
  std::vector<row> rows;
  // A directory hit its time limit (or the round ran out of time).
  bool truncated = false;
  // The spots could not be determined at all.
  std::string error;
};

struct limits {
  // The longest one directory is walked.
  std::chrono::milliseconds per_directory = std::chrono::seconds(30);
  // The longest all the directories of one round are walked together, not
  // counting the pauses between them.
  std::chrono::milliseconds overall = std::chrono::seconds(120);
};

// What walking one directory found.
struct dir_size {
  unsigned long long bytes = 0;
  unsigned long long files = 0;
  // Folders or entries that could not be read (access denied).
  unsigned long long unreadable = 0;
  // The deadline passed: `bytes` is what was counted until then.
  bool timed_out = false;
  // The directory does not exist: no row at all.
  bool missing = false;
  // The directory itself could not be read.
  std::string error;
};

// The seams measure() runs through: the real ones walk the disk on a real
// clock, the tests hand in fakes.
struct hooks {
  // Walk `path`, giving up at `deadline`.
  std::function<dir_size(const std::string &path, walk_clock::time_point deadline)> walk_directory;
  // The size of one file; false when it does not exist (no row), with
  // `error` set when it exists but could not be read.
  std::function<bool(const std::string &path, unsigned long long &size, std::string &error)> file_size;
  // Between two directories: yield to everything else. Returns false when
  // the round should stop (the module is stopping).
  std::function<bool()> pause;
  std::function<walk_clock::time_point()> now;
};

// Collapse spots that name the same path twice (TEMP is often the Windows
// temp directory itself): the first one wins. Trailing separators are
// ignored, and so is case where the platform's paths ignore it.
std::vector<spot> dedupe(const std::vector<spot> &spots, bool ignore_case);

// Measure every spot, in order. A directory gets the smaller of its own limit
// and what is left of the round's; one that starts with nothing left is
// reported without a size.
sample measure(const std::vector<spot> &spots, const limits &lim, const hooks &h);

// Walk one directory tree for its total size: the tree walker of
// check_largest_files (largest_files_scan.hpp), ranking nothing, staying on the
// directory's filesystem and not following links, junctions or mount points.
// Gives up at `deadline`, or as soon as `abort` is set.
dir_size walk_directory(const std::string &path, walk_clock::time_point deadline, const std::atomic<bool> *abort = nullptr);

// The note for a directory walk, empty when the size is complete.
std::string describe(const dir_size &d);

// The swap files (type `file`) of a /proc/swaps listing, header line first,
// with the octal escapes (`\040` for a blank) undone. Swap partitions take no
// filesystem space and are left out.
std::vector<std::string> parse_swap_files(std::istream &in);
std::string unescape_proc_path(const std::string &in);

// The section from the last walk; nothing before the first one.
void add_section(PB::Diagnostics::DiagnosticsMessage *out, const sample &s);

// --- per platform -------------------------------------------------------------

// The spots on this host. `agent_log_dir` is the agent's own log directory
// (${log-path}), measured as one of the log directories. On Windows this also
// reads the shadow-copy storage per volume (as `measured` spots), which is why
// it runs on the walker's thread.
std::vector<spot> discover(const std::string &agent_log_dir);

// The size of one file without opening it (a page file is always open).
bool file_size(const std::string &path, unsigned long long &size, std::string &error);

// Put the calling thread at background I/O (and CPU) priority, best effort.
void lower_priority();

// Whether paths on this platform compare without case.
bool paths_ignore_case();

// --- the thread ---------------------------------------------------------------

// Measures the spots on its own guarded thread: the first round
// `first_delay` after start(), then every `interval`. Configure before
// start(); stop() (and the destructor) abort a walk in progress and join.
class walker {
 public:
  walker() = default;
  ~walker() { stop(); }
  walker(const walker &) = delete;
  walker &operator=(const walker &) = delete;

  std::string agent_log_dir;
  std::chrono::seconds interval = std::chrono::seconds(600);
  std::chrono::seconds first_delay = std::chrono::seconds(15);
  limits lim;

  void start();
  void stop();
  // The last round (captured_at 0 before the first one has finished).
  sample get() const;
  // Start from an earlier walker's last round (a settings reload replaces the
  // walker), so the section does not vanish until the next round.
  void seed(const sample &s);

 private:
  void thread_proc();
  // Waits up to `d`; false when stop() was called.
  bool wait(std::chrono::milliseconds d);

  mutable boost::mutex mutex_;
  sample last_;
  boost::mutex stop_mutex_;
  boost::condition_variable stop_cv_;
  bool stop_requested_ = false;
  // The same request for the walk in progress, polled per entry.
  std::atomic<bool> abort_{false};
  std::shared_ptr<boost::thread> thread_;
};

}  // namespace growth_spots
