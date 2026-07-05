// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/date_time.hpp>
#include <iostream>
#include <nsclient/logger/logger_helper.hpp>
#include <str/format.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>

void nsclient::logging::logger_helper::log_fatal(std::string message) {
  std::cout << message << "\n";
  try {
    std::ofstream stream("nsclient.fatal", std::ios::out | std::ios::app | std::ios::ate);
    stream << message << "\n";
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
