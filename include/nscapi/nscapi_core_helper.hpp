// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <list>
#include <map>
#include <nscapi/dll_defines.hpp>
#include <nscapi/nscapi_core_wrapper.hpp>
#include <string>
#include <vector>

namespace nscapi {
namespace request_builder {
// Serialize a QueryRequestMessage with the caller identity stamped into
// header metadata. Used by core_helper::simple_query{,_as} to attach the
// trusted plugin_id (server-side resolved via plugin_cache) and the
// optional principal (web user, NRPE client tag, ...). Exposed for unit
// testing - keep the helpers and the test in sync; see
// docs/design/core-permissions.md for the wire format.
NSCAPI_EXPORT void build_simple_query_request(int plugin_id, const std::string &principal, const std::string &command, const std::list<std::string> &arguments,
                                              std::string &out_buffer);
NSCAPI_EXPORT void build_simple_query_request(int plugin_id, const std::string &principal, const std::string &command,
                                              const std::vector<std::string> &arguments, std::string &out_buffer);

// Serialize a QueryRequestMessage forwarding an upstream caller's
// identity verbatim. Used by core_helper::simple_query_on_behalf_of
// (the proxy-module path - CheckHelpers and similar). An empty
// caller_plugin_id_str omits the key entirely (the policy then sees the
// principal but no resolvable module).
NSCAPI_EXPORT void build_query_request_on_behalf_of(const std::string &caller_plugin_id_str, const std::string &principal, const std::string &command,
                                                    const std::list<std::string> &arguments, std::string &out_buffer);
}  // namespace request_builder

class NSCAPI_EXPORT core_helper {
  const nscapi::core_wrapper *core_;
  int plugin_id_;

 public:
  core_helper(const nscapi::core_wrapper *core, int plugin_id) : core_(core), plugin_id_(plugin_id) {}
  void register_command(std::string command, std::string description, std::list<std::string> aliases = std::list<std::string>());
  void unregister_command(std::string command);
  void register_alias(std::string command, std::string description, std::list<std::string> aliases = std::list<std::string>());
  void register_event(std::string event);
  void register_channel(std::string channel);

  NSCAPI::nagiosReturn simple_query(std::string command, const std::list<std::string> &argument, std::string &message, std::string &perf,
                                    std::size_t max_length);
  bool simple_query(std::string command, const std::list<std::string> &argument, std::string &result);
  bool simple_query(std::string command, const std::vector<std::string> &argument, std::string &result);

  // simple_query_as: dispatch a query on behalf of a named principal (e.g.
  // an authenticated WEB user). The caller module identity is taken from
  // this core_helper's plugin_id_ - the calling module cannot impersonate
  // a different one. The `principal` is whatever sub-identity the calling
  // module knows about (web user, NRPE client tag, NSCA sender, CLI OS
  // user, ...) and is forwarded verbatim to the core permissions policy.
  // See docs/design/core-permissions.md and the policy decision point in
  // service/plugins/plugin_manager.cpp.
  NSCAPI::nagiosReturn simple_query_as(const std::string &principal, std::string command, const std::list<std::string> &argument, std::string &message,
                                       std::string &perf, std::size_t max_length);
  bool simple_query_as(const std::string &principal, std::string command, const std::list<std::string> &argument, std::string &result);

  // simple_query_on_behalf_of: like simple_query_as, but ALSO overrides
  // the caller module identity. For "proxy" modules (CheckHelpers and
  // similar) that wrap another command on behalf of an upstream caller:
  // the permission check on the downstream call should see the ORIGINAL
  // caller, not the proxy itself.
  //
  // `caller_plugin_id_str` is the verbatim value of the original
  // request's `nscp.caller_plugin_id` metadata (an integer, stringified
  // by core_helper). `principal` is the original principal. When both
  // are empty this degenerates to simple_query (stamps the proxy's own
  // plugin_id). When caller_plugin_id_str is empty but principal is set
  // this degenerates to simple_query_as.
  //
  // Note: this is a "module is trusted to forward truthfully" API. The
  // trust model is the same as for core_helper overall - a malicious
  // module could already do worse by skipping core_helper entirely.
  // Operators who don't want proxy forwarding can deny it via policy
  // (the proxy module's own subject still applies for the first hop).
  NSCAPI::nagiosReturn simple_query_on_behalf_of(const std::string &caller_plugin_id_str, const std::string &principal, std::string command,
                                                 const std::list<std::string> &argument, std::string &message, std::string &perf, std::size_t max_length);
  bool simple_query_on_behalf_of(const std::string &caller_plugin_id_str, const std::string &principal, std::string command,
                                 const std::list<std::string> &argument, std::string &result);

  NSCAPI::nagiosReturn simple_query_from_nrpe(std::string command, const std::string &buffer, std::string &message, std::string &perf, std::size_t max_length);

  // Same as simple_query_from_nrpe but stamps `principal` onto the request
  // so the core permission layer sees `NRPEServer:<principal>` instead of
  // a bare `NRPEServer` subject. Used when the NRPE listener has resolved a
  // sub-identity for the inbound connection (today: client cert Subject DN
  // when `client identity source = cn`).
  NSCAPI::nagiosReturn simple_query_from_nrpe_as(const std::string &principal, std::string command, const std::string &buffer, std::string &message,
                                                 std::string &perf, std::size_t max_length);

  NSCAPI::nagiosReturn exec_simple_command(std::string target, std::string command, const std::list<std::string> &argument, std::list<std::string> &result);
  bool submit_simple_message(std::string channel, std::string source_id, std::string target_id, std::string command, NSCAPI::nagiosReturn code,
                             const std::string &message, const std::string &perf, std::string &response);
  bool emit_event(std::string module, std::string event, std::list<std::map<std::string, std::string> > data, std::string &error);
  bool emit_event(std::string module, std::string event, std::map<std::string, std::string> data, std::string &error);

  typedef std::map<std::string, std::string> storage_map;
  bool put_storage(std::string context, std::string key, std::string value, bool private_data, bool binary_data);
  storage_map get_storage_strings(std::string context);

  bool load_module(std::string name, std::string alias = "");
  bool unload_module(std::string name);

 private:
  const nscapi::core_wrapper *get_core();
};
}  // namespace nscapi