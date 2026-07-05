// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/nscapi_core_wrapper.hpp>

namespace nscapi {
namespace impl {
struct thin_plugin {
  int id_;
  nscapi::core_wrapper* get_core() const;
  inline unsigned int get_id() const { return id_; }
  inline void set_id(const unsigned int id) { id_ = id; }
  // std::string get_base_path() const;
};
}  // namespace impl
}  // namespace nscapi