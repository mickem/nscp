// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/protobuf/command.hpp>
#include <string>

namespace script_wrapper {

// Re-stamp a serialized QueryRequestMessage with the trusted plugin_id.
//
// Used by command_wrapper::query (the raw-protobuf path that Python
// scripts can call directly). A Python script that hand-built its own
// request might have left the metadata blank or set a stale
// nscp.caller_plugin_id; we want the policy decision in plugin_manager
// to see PythonScript as the caller, not whatever the script wrote.
//
// Parse-strip-restamp semantics:
//   - On parse failure: return the request verbatim. A script that
//     deliberately supplies a non-standard payload still works; the
//     core just won't be able to attribute it.
//   - On parse success: drop ALL pre-existing nscp.caller_plugin_id
//     entries from the header metadata and add a single fresh one with
//     plugin_id.
//
// nscp.principal is NOT touched. Python scripts run from the local
// filesystem under operator control - the script has no concept of an
// authenticated end-user principal to forward.
inline std::string restamp_caller_plugin_id(const std::string& request, unsigned int plugin_id) {
  PB::Commands::QueryRequestMessage parsed;
  if (!parsed.ParseFromString(request)) return request;
  auto* header = parsed.mutable_header();
  auto* meta = header->mutable_metadata();
  for (int i = meta->size() - 1; i >= 0; --i) {
    if (meta->Get(i).key() == "nscp.caller_plugin_id") meta->DeleteSubrange(i, 1);
  }
  auto* m = header->add_metadata();
  m->set_key("nscp.caller_plugin_id");
  m->set_value(std::to_string(plugin_id));
  return parsed.SerializeAsString();
}

}  // namespace script_wrapper
