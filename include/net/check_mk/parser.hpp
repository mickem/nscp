// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/noncopyable.hpp>
#include <net/check_mk/data.hpp>

namespace check_mk {
namespace server {
class parser : public boost::noncopyable {
  std::vector<char> buffer_;

 public:
};
}  // namespace server
}  // namespace check_mk