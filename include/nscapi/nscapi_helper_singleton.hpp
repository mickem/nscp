// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/dll_defines.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>

namespace nscapi {
class NSCAPI_EXPORT helper_singleton {
  core_wrapper* core_;

 public:
  helper_singleton();
  core_wrapper* get_core() const { return core_; }
};

extern helper_singleton* plugin_singleton;
}  // namespace nscapi