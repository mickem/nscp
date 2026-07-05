// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <exception>
#include <string>

namespace dll {
class dll_exception final : public std::exception {
  std::string what_;

 public:
  explicit dll_exception(const std::string& what) : what_(what) {}
  ~dll_exception() throw() override {}
  const char* what() const throw() override { return what_.c_str(); }
};
}  // namespace dll

#ifdef WIN32
#include <dll/impl_w32.hpp>
#else
#include <dll/impl_unix.hpp>
#endif
namespace dll {
#ifdef WIN32
typedef ::dll::win32::impl dll_impl;
#else
typedef dll::iunix::impl dll_impl;
#endif
}  // namespace dll