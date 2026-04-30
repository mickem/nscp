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

#include <bytes/base64.h>
#include <string>

namespace bytes {

inline std::string base64_encode(const void *data, std::size_t size) {
  if (size == 0) return {};
  const std::size_t len = b64::b64_encode(data, size, nullptr, 0);
  std::string out(len, '\0');
  b64::b64_encode(data, size, &out[0], len);
  return out;
}

inline std::string base64_encode(const std::string &input) { return base64_encode(input.data(), input.size()); }

}  // namespace bytes
