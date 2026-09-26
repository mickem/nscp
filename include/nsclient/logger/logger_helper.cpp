// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <atomic>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <mutex>
#include <nsclient/logger/logger_helper.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <vector>

#ifndef WIN32
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

namespace {
// Published as a pointer, and the string it points at is never freed or
// mutated: log_fatal() runs from a terminate handler, where a mutex could
// deadlock against the thread that is already dying, and a std::string
// reassigned under a concurrent read is a torn read. One atomic pointer load
// has neither problem, and leaking one path at shutdown costs nothing.
std::atomic<const std::string *> fatal_file_path{nullptr};

// Every path set_fatal_file() has replaced. They still must not be freed - a
// reader may hold one - but they have to stay reachable, or LeakSanitizer
// reports each overwritten one as a leak. The list is itself never destroyed,
// so no static destructor can free a path under a late reader either. Only
// the setter touches it, never the terminate handler, so a mutex is fine.
std::mutex retired_mutex;
std::vector<const std::string *> *retired_fatal_file_paths = new std::vector<const std::string *>();

void publish_fatal_file(const std::string &path) {
  const std::string *previous = fatal_file_path.exchange(new std::string(path), std::memory_order_acq_rel);
  if (previous == nullptr) return;
  std::lock_guard<std::mutex> lock(retired_mutex);
  retired_fatal_file_paths->push_back(previous);
}

std::string current_fatal_file() {
  const std::string *path = fatal_file_path.load(std::memory_order_acquire);
  return path ? *path : std::string("nsclient.fatal");
}

// Open `file` for append, creating it private to this account, and say
// whether that worked. Checking is the whole point: a default-constructed
// ofstream has exceptions disabled, so a failed open only sets failbit and
// every subsequent << is a silent no-op - a report into a directory that does
// not exist looked exactly like a report that was written.
//
// On POSIX this is open(2) rather than ofstream so the flags can be spelled
// out. O_NOFOLLOW refuses a symlink planted as the final component instead of
// following it; that matters because the fallback below writes into the temp
// folder, as root, for a service. 0600 keeps the report - which quotes an
// exception message and may carry internals - out of other users' reach, and
// O_CLOEXEC keeps the descriptor out of any child a dying agent spawns.
// O_APPEND makes each line a single atomic write, so two threads reporting at
// once cannot interleave mid-line.
bool append_line(const std::string &file, const std::string &message) {
  if (file.empty()) return false;
#ifdef WIN32
  std::ofstream stream(file.c_str(), std::ios::out | std::ios::app | std::ios::ate);
  if (!stream.is_open()) return false;
  stream << message << "\n";
  stream.flush();
  return stream.good();
#else
  const int fd = ::open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (fd < 0) return false;
  const std::string line = message + "\n";
  std::size_t written = 0;
  bool ok = true;
  while (written < line.size()) {
    const ssize_t n = ::write(fd, line.data() + written, line.size() - written);
    if (n < 0) {
      if (errno == EINTR) continue;
      ok = false;
      break;
    }
    written += static_cast<std::size_t>(n);
  }
  ::close(fd);
  return ok;
#endif
}

// Can a report be appended to `file`? Opens it exactly the way append_line()
// does and closes it again without writing, so probing a file that already
// holds a report does not append a blank line to it.
bool can_append(const std::string &file) {
  if (file.empty()) return false;
#ifdef WIN32
  const std::ofstream stream(file.c_str(), std::ios::out | std::ios::app | std::ios::ate);
  return stream.is_open();
#else
  const int fd = ::open(file.c_str(), O_WRONLY | O_CREAT | O_APPEND | O_NOFOLLOW | O_CLOEXEC, S_IRUSR | S_IWUSR);
  if (fd < 0) return false;
  ::close(fd);
  return true;
#endif
}

// A directory of our own inside the system temp folder, or empty if we cannot
// have one.
//
// The fallback never writes straight into the temp folder. On Linux that is
// /tmp: world-writable, and appending to a fixed name there as root means
// appending to whatever an unprivileged user pre-created or symlinked, which
// is a file-overwrite primitive handed out by the very feature that is
// supposed to make a crash easier to diagnose. fs.protected_symlinks and
// fs.protected_regular blunt it on a modern kernel, but a fallback whose
// safety rests on a sysctl default is not one worth shipping.
//
// mkdir(0700) sets the mode in the same syscall that creates the directory,
// so there is no create-then-chmod window to slip into, and the name carries
// the uid so two accounts running the agent do not collide. If something is
// already there it is used only if it really is a directory, really is ours,
// is not a symlink, and is not accessible to anyone else; otherwise we have
// nowhere safe to write and say so rather than writing anyway.
std::string private_temp_dir() {
  try {
    boost::system::error_code ec;
    const boost::filesystem::path tmp = boost::filesystem::temp_directory_path(ec);
    if (ec) return std::string();
#ifdef WIN32
    // %TEMP% is already per-account on Windows (and admin-only for a
    // service), so the subdirectory is about tidiness rather than safety.
    const boost::filesystem::path dir = tmp / "nsclient++";
    boost::filesystem::create_directory(dir, ec);
    if (!boost::filesystem::is_directory(dir, ec)) return std::string();
    return dir.string();
#else
    const boost::filesystem::path dir = tmp / ("nsclient++-" + std::to_string(static_cast<unsigned long>(::geteuid())));
    const std::string path = dir.string();
    if (::mkdir(path.c_str(), S_IRWXU) != 0 && errno != EEXIST) return std::string();
    struct stat st;
    if (::lstat(path.c_str(), &st) != 0) return std::string();
    if (!S_ISDIR(st.st_mode)) return std::string();
    if (st.st_uid != ::geteuid()) return std::string();
    if ((st.st_mode & (S_IRWXG | S_IRWXO)) != 0) return std::string();
    return path;
#endif
  } catch (...) {
    return std::string();
  }
}

// Where a report goes when the configured file cannot be written: the same
// file name, in a directory of our own under the system temp folder. A report
// nobody expected to find there still beats no report at all - the copy on
// stdout goes nowhere under the SCM.
std::string temp_fallback(const std::string &file) {
  try {
    const std::string dir = private_temp_dir();
    if (dir.empty()) return std::string();
    std::string name = boost::filesystem::path(file).filename().string();
    if (name.empty() || name == "." || name == "..") name = "nsclient.fatal";
    return (boost::filesystem::path(dir) / name).string();
  } catch (...) {
    return std::string();
  }
}

// Create a file's parent directory, and say whether that changed anything -
// i.e. whether retrying an append is worth doing.
bool create_missing_parent(const std::string &file) {
  try {
    boost::system::error_code ec;
    const boost::filesystem::path parent = boost::filesystem::path(file).parent_path();
    if (parent.empty() || boost::filesystem::exists(parent, ec)) return false;
    boost::filesystem::create_directories(parent, ec);
    return !ec && boost::filesystem::is_directory(parent, ec);
  } catch (...) {
    return false;
  }
}

enum class writability {
  // A report can be appended right now.
  ok,
  // The folder is not there yet, which is not a failure: log_fatal() creates
  // it if it ever has something to write.
  deferred,
  // The open failed for a reason creating a folder will not fix.
  no
};

// Can a report be appended to `file`? Opens it exactly the way log_fatal()
// will, because only the open answers the question: the folder may be
// read-only, on a full disk, or not a folder at all.
//
// A missing parent directory is deliberately NOT created here. This runs on
// every start, including every short-lived command line invocation, and the
// agent has nothing to report yet - eagerly creating ${log-path} would leave
// a folder behind for a report that is never written (the fleet-sync hostile
// test holds the agent to creating nothing it did not need to). The ordinary
// log creates that folder on its first line anyway, long before anything can
// crash, and log_fatal() creates it itself if it gets there first.
//
// A probe that had to create the file removes it again. An nsclient.fatal
// that exists means something was reported, and an empty one appearing on
// every boot would be a standing false alarm.
writability is_writable(const std::string &file) {
  if (file.empty()) return writability::no;
  try {
    boost::system::error_code ec;
    const boost::filesystem::path path(file);
    const boost::filesystem::path parent = path.parent_path();
    if (!parent.empty() && !boost::filesystem::exists(parent, ec)) return writability::deferred;
    const bool existed = boost::filesystem::exists(path, ec);
    if (!can_append(file)) return writability::no;
    if (!existed) boost::filesystem::remove(path, ec);
    return writability::ok;
  } catch (...) {
    return writability::no;
  }
}
}  // namespace

std::string nsclient::logging::logger_helper::fatal_file() { return current_fatal_file(); }

std::string nsclient::logging::logger_helper::fatal_fallback_file(const std::string &file) { return temp_fallback(file); }

std::string nsclient::logging::logger_helper::set_fatal_file(const std::string &path) {
  if (path.empty()) return current_fatal_file();
  if (is_writable(path) != writability::no) {
    publish_fatal_file(path);
    return path;
  }
  const std::string fallback = temp_fallback(path);
  if (!fallback.empty() && is_writable(fallback) != writability::no) {
    publish_fatal_file(fallback);
    return fallback;
  }
  // Neither worked. Keep whatever was configured before rather than pointing
  // the channel at somewhere known to be unwritable; the caller logs the
  // difference between what it asked for and what it got.
  return current_fatal_file();
}

void nsclient::logging::logger_helper::log_fatal(std::string message) {
  std::cout << message << "\n";
  try {
    const std::string file = current_fatal_file();
    if (append_line(file, message)) return;
    // The folder may simply not be there yet - this is the first thing the
    // agent has had to write, and set_fatal_file() deliberately does not
    // create it up front. Create it now and try once more.
    if (create_missing_parent(file) && append_line(file, message)) return;
    // Still nothing: the folder was removed, the disk filled up, the
    // permissions changed - or nothing ever called set_fatal_file() and the
    // working directory is not writable, which for a service is wherever the
    // SCM happened to start it. This is the one report that explains why the
    // agent is going down, so try the temp folder before dropping it.
    const std::string fallback = temp_fallback(file);
    if (!fallback.empty() && fallback != file) append_line(fallback, message);
  } catch (...) {
    // ignored, since it has also been logged to display...
  }
}

std::string nsclient::logging::logger_helper::render_log_level_short(::PB::Log::LogEntry::Entry::Level l) {
  if (l == ::PB::Log::LogEntry_Entry_Level_LOG_CRITICAL) {
    return "C";
  } else if (l == ::PB::Log::LogEntry_Entry_Level_LOG_ERROR) {
    return "E";
  } else if (l == ::PB::Log::LogEntry_Entry_Level_LOG_WARNING) {
    return "W";
  } else if (l == ::PB::Log::LogEntry_Entry_Level_LOG_INFO) {
    return "L";
  } else if (l == ::PB::Log::LogEntry_Entry_Level_LOG_DEBUG) {
    return "D";
  } else if (l == ::PB::Log::LogEntry_Entry_Level_LOG_TRACE) {
    return "T";
  } else {
    return "?";
  }
}

std::string nsclient::logging::logger_helper::render_log_level_long(::PB::Log::LogEntry::Entry::Level l) {
  if (l == ::PB::Log::LogEntry_Entry_Level_LOG_CRITICAL) {
    return "critical";
  } else if (l == ::PB::Log::LogEntry_Entry_Level_LOG_ERROR) {
    return "error";
  } else if (l == ::PB::Log::LogEntry_Entry_Level_LOG_WARNING) {
    return "warning";
  } else if (l == ::PB::Log::LogEntry_Entry_Level_LOG_INFO) {
    return "info";
  } else if (l == ::PB::Log::LogEntry_Entry_Level_LOG_DEBUG) {
    return "debug";
  } else if (l == ::PB::Log::LogEntry_Entry_Level_LOG_TRACE) {
    return "trace";
  } else {
    return "unknown";
  }
}

std::pair<bool, std::string> nsclient::logging::logger_helper::render_console_message(const bool oneline, const std::string &data) {
  try {
    std::stringstream ss;
    bool is_error = false;
    PB::Log::LogEntry message;
    if (data.empty() || !message.ParseFromString(data)) {
      log_fatal("Failed to parse message: " + str::format::strip_ctrl_chars(data));
      return std::make_pair(true, "ERROR");
    }

    for (int i = 0; i < message.entry_size(); i++) {
      const ::PB::Log::LogEntry::Entry &msg = message.entry(i);
      std::string tmp = msg.message();
      str::utils::replace(tmp, "\n", "\n    -    ");
      if (oneline) {
        ss << msg.file() << "(" << msg.line() << "): " << render_log_level_long(msg.level()) << ": " << tmp << "\n";
      } else {
        if (i > 0) ss << " -- ";
        ss << str::format::lpad(render_log_level_short(msg.level()), 1) << " " << str::format::rpad(msg.sender(), 10) << " " + msg.message() << "\n";
        if (msg.level() == ::PB::Log::LogEntry_Entry_Level_LOG_ERROR) {
          ss << "                    " << msg.file() << ":" << msg.line() << "\n";
        }
      }
    }
#ifdef WIN32
    return std::make_pair(is_error, utf8::to_encoding(ss.str(), ""));
#else
    return std::make_pair(is_error, ss.str());
#endif
  } catch (std::exception &e) {
    log_fatal("Failed to parse data from: " + str::format::strip_ctrl_chars(data) + ": " + e.what());
  } catch (...) {
    log_fatal("Failed to parse data from: " + str::format::strip_ctrl_chars(data));
  }
  return std::make_pair(true, "ERROR");
}

std::string nsclient::logging::logger_helper::get_formated_date(const std::string &format) {
  std::stringstream ss;
  auto *facet = new boost::posix_time::time_facet(format.c_str());
  ss.imbue(std::locale(std::cout.getloc(), facet));
  ss << boost::posix_time::second_clock::local_time();
  return ss.str();
}
