// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

namespace nsclient {
namespace logging {

struct log_message_factory {
  static void log_fatal(const std::string &message);

  static std::string create_critical(const std::string &module, const char *file, const int line, const std::string &message);
  static std::string create_error(const std::string &module, const char *file, const int line, const std::string &message);
  static std::string create_warning(const std::string &module, const char *file, const int line, const std::string &message);
  static std::string create_info(const std::string &module, const char *file, const int line, const std::string &message);
  static std::string create_debug(const std::string &module, const char *file, const int line, const std::string &message);
  static std::string create_trace(const std::string &module, const char *file, const int line, const std::string &message);
};

}  // namespace logging
}  // namespace nsclient