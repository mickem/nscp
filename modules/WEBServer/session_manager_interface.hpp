#pragma once

#include <Request.h>
#include <StreamResponse.h>

#include <list>
#include <memory>
#include <net/socket/allowed_hosts.hpp>
#include <string>
#include <vector>

#include "auth_rate_limiter.hpp"
#include "error_handler_interface.hpp"
#include "metrics_handler.hpp"
#include "token_store.hpp"
#include "user_manager.h"
struct session_manager_interface {
 private:
  std::unique_ptr<error_handler_interface> log_data;

  metrics_handler metrics_store;
  token_store tokens;
  socket_helpers::allowed_hosts_manager allowed_hosts;
  user_manager users;
  auth_rate_limiter rate_limiter;
  // The "anonymous" role is a deliberate opt-in to expose endpoints without
  // authentication. Default false: any role named `anonymous` registered via
  // settings is silently ignored, and `can()` does not consult the anonymous
  // grant table when no user is logged in. An operator who genuinely wants
  // anonymous access has to flip this on AND register the role - one
  // accidental knob is not enough to expose anything.
  bool allow_anonymous_ = false;

  // Case-insensitive substrings matched against a request's User-Agent. When
  // any pattern matches, the request is allowed to authenticate via the
  // legacy `?password=...` / `?TOKEN=...` query-string mechanism that was
  // otherwise removed for security (commit 340b8db1). Defaults to
  // {kDefaultLegacyQueryAuthUserAgents} so that Icinga's bundled
  // check_nscp_api keeps working without forcing every integration site to
  // update their plugin in lockstep with NSClient. Set to an empty list to
  // disable the fallback entirely.
  std::vector<std::string> legacy_query_auth_user_agents_;

 public:
  // The single source of truth for the default allowlist. Used both by the
  // settings layer (so `show-default` reports it accurately) and by the
  // constructor (so `session_manager_interface` constructed without going
  // through settings registration — e.g. in tests — still has Icinga
  // working out of the box). Keeping the literal in one place keeps the
  // two paths from drifting apart.
  static constexpr const char *kDefaultLegacyQueryAuthUserAgents = "Icinga/check_nscp_api";

  session_manager_interface();

  bool process_auth_header(const std::string &grant, Mongoose::Request &request, Mongoose::StreamResponse &response);
  bool is_logged_in(const std::string &grant, Mongoose::Request &request, Mongoose::StreamResponse &response);

  bool is_allowed(const std::string &ip);

  bool validate_token(const std::string &token);
  void revoke_token(const std::string &token);
  void revoke_tokens_for_user(const std::string &user);
  std::string generate_token(const std::string &user);

  std::string get_metrics();
  std::string get_metrics_v2();
  std::string get_open_metrics();
  void set_metrics(const std::string &metrics, const std::string &metrics_list, std::list<std::string> open_metrics);

  void add_log_message(bool is_error, const error_handler_interface::log_entry &entry) const;
  error_handler_interface *get_log_data() const;
  void reset_log() const;

  void set_allowed_hosts(const std::string &host);
  void set_allowed_hosts_cache(bool value);
  void set_allow_anonymous(bool value) { allow_anonymous_ = value; }
  void set_auth_rate_limit_max_failures(int value) { rate_limiter.set_max_failures(value); }
  void set_auth_rate_limit_block_seconds(int value) { rate_limiter.set_block_seconds(value); }

  // Comma-separated list of substrings; clients whose User-Agent header
  // contains any of these (case-insensitive) are permitted to use the legacy
  // query-string credential / token fallback. Empty string disables it.
  void set_legacy_query_auth_user_agents(const std::string &csv);

  // Returns true when the given User-Agent matches any configured pattern,
  // i.e. when this client is allowed to pass credentials/token in the query
  // string. Public so legacy_controller can apply the same check at the
  // endpoint level.
  bool client_allows_legacy_query_auth(const std::string &user_agent) const;

  std::list<std::string> boot();
  bool validate_user(const std::string &user, const std::string &password);
  void store_user_in_response(const std::string &user, Mongoose::StreamResponse &response);
  void store_token_in_response(const std::string &token, Mongoose::StreamResponse &response) const;
  bool can(const std::string &grant, Mongoose::StreamResponse &response);
  static void get_user_from_response(const Mongoose::StreamResponse &response, std::string &user, std::string &key);
  void add_user(const std::string &user, const std::string &role, const std::string &password);
  bool has_user(const std::string &user) const;
  void add_grant(const std::string &role, const std::string &grant);
};
