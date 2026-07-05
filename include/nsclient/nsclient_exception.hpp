// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <exception>
#include <string>
#include <utility>

namespace nsclient {

class nsclient_exception final : public std::exception {
  std::string error_;

 public:
  explicit nsclient_exception(std::string error) noexcept : error_(std::move(error)) {};
  nsclient_exception(const nsclient_exception &other) noexcept : error_(other.error_) {}
  ~nsclient_exception() noexcept override = default;

  std::string reason() const noexcept { return error_; }
  const char *what() const noexcept override { return error_.c_str(); }
};
}  // namespace nsclient
