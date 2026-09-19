// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <dotnet/runtime.hpp>

namespace fs = boost::filesystem;

namespace dotnet {

namespace {

template <typename Fn>
bool resolve_one(host &host, const fs::path &assembly, const char *name, Fn &out, std::string &error) {
  void *fn = host.get_function(assembly, bridge_type_name, name, error);
  if (fn == nullptr) return false;
  out = reinterpret_cast<Fn>(fn);
  return true;
}

}  // namespace

bool resolve_bridge(host &host, const fs::path &assembly, bridge_functions &out, std::string &error) {
  bridge_functions fns;
  const bool ok = resolve_one(host, assembly, "Load", fns.load, error) && resolve_one(host, assembly, "Start", fns.start, error) &&
                  resolve_one(host, assembly, "Unload", fns.unload, error) && resolve_one(host, assembly, "Describe", fns.describe, error) &&
                  resolve_one(host, assembly, "Query", fns.query, error) && resolve_one(host, assembly, "Submit", fns.submit, error) &&
                  resolve_one(host, assembly, "Exec", fns.exec, error) && resolve_one(host, assembly, "Message", fns.message, error) &&
                  resolve_one(host, assembly, "HasMessageHandler", fns.has_message, error);
  if (!ok) return false;
  out = fns;
  return true;
}

void NSCP_DOTNET_CALL append_to_string(void *wctx, const std::uint8_t *data, std::int32_t len) {
  if (wctx == nullptr || data == nullptr || len <= 0) return;
  static_cast<std::string *>(wctx)->append(reinterpret_cast<const char *>(data), static_cast<std::size_t>(len));
}

std::string describe_plugin(managed_describe_fn describe, void *handle) {
  std::string out;
  if (describe && handle) describe(handle, &append_to_string, &out);
  return out;
}

}  // namespace dotnet
