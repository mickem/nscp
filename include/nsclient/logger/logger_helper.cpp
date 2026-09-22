// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <atomic>
#include <boost/date_time.hpp>
#include <boost/filesystem.hpp>
#include <fstream>
#include <iostream>
#include <nsclient/logger/logger_helper.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>

namespace {
// Published as a pointer, and the string it points at is never freed or
// mutated: log_fatal() runs from a terminate handler, where a mutex could
// deadlock against the thread that is already dying, and a std::string
// reassigned under a concurrent read is a torn read. One atomic pointer load
// has neither problem, and leaking one path at shutdown costs nothing.
std::atomic<const std::string *> fatal_file_path{nullptr};

std::string current_fatal_file() {
  const std::string *path = fatal_file_path.load(std::memory_order_acquire);
  return path ? *path : std::string("nsclient.fatal");
}

// Append one line, and say whether it actually landed. The check is the whole
// point: a default-constructed ofstream has exceptions disabled, so a failed
// open only sets failbit and every subsequent << is a silent no-op. Without
// it, a report into a directory that does not exist looks exactly like a
// report that was written.
bool append_line(const std::string &file, const std::string &message) {
  if (file.empty()) return false;
  std::ofstream stream(file.c_str(), std::ios::out | std::ios::app | std::ios::ate);
  if (!stream.is_open()) return false;
  stream << message << "\n";
  stream.flush();
  return stream.good();
}

// The same file name in the system temp folder. This is the place of last
// resort when the configured folder cannot be written: the temp folder is
// writable for whatever account the service runs under on both Windows and
// Linux, and a report nobody expected to find there still beats no report at
// all - the message is on stdout as well, which under the SCM goes nowhere.
std::string temp_fallback(const std::string &file) {
  try {
    boost::system::error_code ec;
    const boost::filesystem::path tmp = boost::filesystem::temp_directory_path(ec);
    if (ec) return std::string();
    std::string name = boost::filesystem::path(file).filename().string();
    if (name.empty() || name == "." || name == "..") name = "nsclient.fatal";
    return (tmp / name).string();
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
    {
      std::ofstream probe(file.c_str(), std::ios::out | std::ios::app | std::ios::ate);
      if (!probe.is_open()) return writability::no;
    }
    if (!existed) boost::filesystem::remove(path, ec);
    return writability::ok;
  } catch (...) {
    return writability::no;
  }
}
}  // namespace

std::string nsclient::logging::logger_helper::set_fatal_file(const std::string &path) {
  if (path.empty()) return current_fatal_file();
  if (is_writable(path) != writability::no) {
    fatal_file_path.store(new std::string(path), std::memory_order_release);
    return path;
  }
  const std::string fallback = temp_fallback(path);
  if (!fallback.empty() && is_writable(fallback) != writability::no) {
    fatal_file_path.store(new std::string(fallback), std::memory_order_release);
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
