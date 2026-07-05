// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

namespace nscapi {

struct log_handler {
  virtual ~log_handler() = default;
  virtual void log_debug(std::string module, std::string file, int line, std::string msg) const = 0;
  virtual void log_error(std::string module, std::string file, int line, std::string msg) const = 0;
};
}  // namespace nscapi