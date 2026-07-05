// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#ifdef WIN32
#include <error/error_w32.hpp>
#ifndef FORMAT_MESSAGE_IGNORE_INSERTS
#define FORMAT_MESSAGE_IGNORE_INSERTS 0x00000200
#endif
#else
#include <string.h>

#include <cstdarg>
#include <cstddef>
#include <cstdio>
#endif

#include <string>

namespace error {
class format {
 public:
#ifdef WIN32
  static std::string from_system(const unsigned long dwError) { return win32::format_message(0, "", dwError); }
  static std::string from_module(std::string module, const unsigned long dwError) {
    return win32::format_message(FORMAT_MESSAGE_IGNORE_INSERTS, module, dwError);
  }
  static std::string from_system(const unsigned long dwError, unsigned long *arguments) { return win32::format_message(0, "", dwError, arguments); }
  static std::string from_module(std::string module, const unsigned long dwError, unsigned long *arguments) {
    return win32::format_message(0, module, dwError, arguments);
  }
  class message {
   public:
    static std::string from_module(std::string module, const unsigned long dwError) {
      return win32::format_message(FORMAT_MESSAGE_IGNORE_INSERTS, module, dwError);
    }
    static std::string from_system(unsigned long dwError, unsigned long *arguments) { return win32::format_message(0, "", dwError, arguments); }
  };
#else
  static std::string from_system(int dwError) {
    char buf[1024] = {0};
    // strerror_r has two incompatible signatures (GNU vs XSI/POSIX). Dispatch via overload.
    return apply(buf, sizeof(buf), ::strerror_r(dwError, buf, sizeof(buf)), dwError);
  }

 private:
  // GNU strerror_r: returns char* (may point to internal static buffer or to buf).
  static std::string apply(char *buf, std::size_t buflen, const char *result, int dwError) {
    if (result == nullptr) {
      char tmp[64];
      std::snprintf(tmp, sizeof(tmp), "Unknown error %d", dwError);
      return tmp;
    }
    (void)buf;
    (void)buflen;
    return std::string(result);
  }
  // XSI/POSIX strerror_r: returns int (0 on success, errno on failure).
  static std::string apply(char *buf, std::size_t buflen, int rc, int dwError) {
    if (rc != 0) {
      std::snprintf(buf, buflen, "Unknown error %d", dwError);
    }
    return std::string(buf);
  }

 public:
#endif
};
class lookup {
 public:
#ifdef WIN32
  static std::string last_error(unsigned long dwLastError = -1) { return win32::format_message(0, "", dwLastError == -1 ? win32::lookup() : dwLastError); }
#else
  static std::string last_error(int dwLastError = -1) { return ::error::format::from_system(dwLastError); }
#endif
};
}  // namespace error
