// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <parsers/where/dll_defines.hpp>

namespace parsers {
namespace where {
struct constants {
  static NSCAPI_EXPORT long long now;
  static NSCAPI_EXPORT long long get_now();
  static NSCAPI_EXPORT void reset();
};
}  // namespace where
}  // namespace parsers