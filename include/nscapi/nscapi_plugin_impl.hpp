// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <memory>
#include <nscapi/nscapi_core_wrapper.hpp>

namespace nscapi {
namespace impl {
struct simple_plugin {
  int id_;
  core_wrapper* get_core() const;
  int get_id() const { return id_; }
  void set_id(const int id) { id_ = id; }
  std::string get_base_path() const;
};
}  // namespace impl
}  // namespace nscapi
