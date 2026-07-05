// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <bytes/buffer.hpp>
#include <cstring>
#include <string>

namespace hlp {
class tchar_buffer : public buffer<wchar_t> {
 public:
  tchar_buffer(const std::wstring& str) : buffer(str.length() + 2) {
#ifdef _MSC_VER
    wcsncpy_s(get(), size(), str.c_str(), str.length());
#else
    wcsncpy(get(), str.c_str(), str.length());
#endif
  }
  tchar_buffer(const std::size_t len) : buffer(len) {}
  void zero() const {
    if (size() > 1) memset(get(), 0, size());
  }
};

class char_buffer : public buffer<char> {
 public:
  explicit char_buffer(const std::string& str) : buffer(str.length() + 2) {
#ifdef _MSC_VER
    strncpy_s(get(), size(), str.c_str(), str.length());
#else
    strncpy(get(), str.c_str(), str.length());
#endif
  }
  explicit char_buffer(const std::size_t len) : buffer(len) {}
  void zero() const {
    if (size() > 1) memset(get(), 0, size());
  }
  std::string str() const { return std::string(get(), size()); }
};

}  // namespace hlp