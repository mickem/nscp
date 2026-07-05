// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <map>
#include <nscapi/protobuf/command.hpp>
#include <string>

#include "filter_config_object.hpp"

namespace memory_checks {

namespace realtime {

struct mem_filter_helper_wrapper;
struct helper {
  mem_filter_helper_wrapper *memory_helper;

  helper(nscapi::core_wrapper *core, int plugin_id);
  void add_obj(std::shared_ptr<filters::mem::filter_config_object> object);
  void boot();
  void check();
  std::map<std::string, long long> get_counts() const;
};
}  // namespace realtime
namespace memory {

void check(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
}
}  // namespace memory_checks