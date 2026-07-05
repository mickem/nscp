// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <exception>
#include <str/utf8.hpp>
#include <string>

namespace check_net {

// Format an exception message for inclusion in user-facing check output.
//
// Two clean-ups are applied:
// 1. Convert from the OS code page to UTF-8 (Boost.Asio surfaces system error
//    messages localized in the OS code page, e.g. Swedish on Windows).
// 2. Strip the trailing source-location annotation that newer Boost.Asio
//    appends, e.g.
//      "resolve: ... [system:11001 at C:/.../resolver_service.hpp:84]"
//    -> "resolve: ..."
//    The build path is noisy and not useful to operators.
inline std::string format_exception_message(const std::exception &e) {
  std::string msg = utf8::utf8_from_native(e.what());
  // Trim a trailing "[...]" block introduced by Boost (and similar libs) that
  // includes file/line information.
  if (!msg.empty() && msg.back() == ']') {
    const auto open = msg.rfind('[');
    if (open != std::string::npos) {
      // Drop the bracket block and any whitespace immediately preceding it.
      auto end = open;
      while (end > 0 && (msg[end - 1] == ' ' || msg[end - 1] == '\t')) --end;
      msg.erase(end);
    }
  }
  return msg;
}

}  // namespace check_net
