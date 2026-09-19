// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

// The pieces every native module that hosts managed plugins needs on top of
// dotnet::host: the table of bridge entry points, resolving it out of
// NSCP.Core.dll and the little helpers for moving bytes across the C ABI.
// Shared by modules/DotnetPlugins and modules/PowerShellScript.

#include <boost/filesystem/path.hpp>
#include <cstdint>
#include <dotnet/bridge.hpp>
#include <dotnet/host.hpp>
#include <string>

namespace dotnet {

// The managed entry points of NSCP.Core.Native.Bridge, as resolved by
// resolve_bridge(). All null until then.
struct bridge_functions {
  managed_load_fn load = nullptr;
  managed_start_fn start = nullptr;
  managed_unload_fn unload = nullptr;
  managed_describe_fn describe = nullptr;
  managed_query_fn query = nullptr;
  managed_submit_fn submit = nullptr;
  managed_exec_fn exec = nullptr;
  managed_message_fn message = nullptr;
  managed_has_message_fn has_message = nullptr;

  bool resolved() const { return load != nullptr; }
};

// Resolve every entry point of the bridge from `assembly` (NSCP.Core.dll).
// Leaves `out` untouched and fills `error` when any of them is missing.
bool resolve_bridge(host &host, const boost::filesystem::path &assembly, bridge_functions &out, std::string &error);

// write_fn appending to a std::string handed through `wctx`: the response sink
// the managed side writes through.
void NSCP_DOTNET_CALL append_to_string(void *wctx, const std::uint8_t *data, std::int32_t len);

// Collect what `describe` writes for `handle` ("name\nversion\ndescription").
std::string describe_plugin(managed_describe_fn describe, void *handle);

inline const std::uint8_t *bytes_of(const std::string &s) { return reinterpret_cast<const std::uint8_t *>(s.data()); }
inline std::int32_t length_of(const std::string &s) { return static_cast<std::int32_t>(s.size()); }

}  // namespace dotnet
