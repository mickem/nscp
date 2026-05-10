/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

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