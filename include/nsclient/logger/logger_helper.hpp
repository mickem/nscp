// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/log.hpp>
#include <string>

namespace nsclient {
namespace logging {

struct logger_helper {
  static std::string get_formated_date(const std::string &format);
  static void log_fatal(std::string message);
  static std::pair<bool, std::string> render_console_message(bool oneline, const std::string &data);
  static std::string render_log_level_short(PB::Log::LogEntry::Entry::Level l);
  static std::string render_log_level_long(PB::Log::LogEntry::Entry::Level l);
};

}  // namespace logging
}  // namespace nsclient