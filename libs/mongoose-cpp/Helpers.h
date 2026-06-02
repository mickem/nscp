// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <string>

#include "dll_defines.hpp"

/**
 * A stream response to a request
 */
namespace Mongoose {
struct NSCAPI_EXPORT Helpers {
  static std::string encode_b64(const std::string &str);
  static std::string decode_b64(const std::string &str);
};
}  // namespace Mongoose
