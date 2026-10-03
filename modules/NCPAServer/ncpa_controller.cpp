// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ncpa_controller.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <list>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <utility>

namespace {
// Answered for a wrong or missing token, and for any request while no token is
// configured: one wording for all three, so a caller learns nothing about
// which it was. The log says which.
const char *const kBadCredentials = "Incorrect credentials given.";
const char *const kBlocked = "Too many failed authentication attempts, try again later.";

void answer_json(Mongoose::StreamResponse &response, const std::string &body) {
  response.setCodeOk();
  response.setHeader("Content-Type", "application/json");
  response.append(body);
}

std::string join_path(const std::vector<std::string> &path) { return "/api/" + boost::algorithm::join(path, "/"); }
}  // namespace

ncpa_controller::ncpa_controller(ncpa_config config, std::shared_ptr<ncpa_sources> sources)
    : RegexpController("/api"), config_(std::move(config)), sources_(std::move(sources)) {
  rate_limiter_.set_max_failures(config_.auth_max_failures);
  rate_limiter_.set_block_seconds(config_.auth_block_seconds);

  // `/api`, `/api/` and `/api/<anything>`, but not `/apifoo`: the prefix match
  // the base class does on its own would take that too. The capture is the
  // raw, still percent-encoded path below /api, which is split before it is
  // decoded (see ncpa::split_path). NCPA accepts both verbs, with the
  // parameters in the query string or in a form body.
  addRoute("GET", "^(?:/(.*))?$", this, &ncpa_controller::api);
  addRoute("POST", "^(?:/(.*))?$", this, &ncpa_controller::api);
}

bool ncpa_controller::authenticate(const Mongoose::Request &request, const ncpa::form_vector &args, Mongoose::StreamResponse &response) {
  // `allowed hosts` is not checked here: the server refuses those peers as
  // it accepts them (NCPAServer's accept filter), before the handshake.
  const std::string &remote = request.getRemoteIp();
  // One step under a lock: the block check, the comparison and the count.
  // With several workers, parallel guesses from one host used to all pass
  // is_blocked() before any of them was counted, getting about twice the
  // documented number of tries per block. The comparison is in-memory and
  // constant-time, so holding the lock across it costs nothing. The log line
  // is only composed under it and written after: logging goes through the
  // core and must not serialise every other request behind it.
  std::string failure;
  bool blocked = false;
  {
    const std::lock_guard<std::mutex> auth_lock(auth_mutex_);
    if (rate_limiter_.is_blocked(remote)) {
      blocked = true;
      failure = "blocked after repeated failed tokens.";
    } else {
      switch (ncpa::check_token(ncpa::form_value(args, "token"), config_.token, config_.backup_token)) {
        case ncpa::token_result::accepted:
          rate_limiter_.record_success(remote);
          return true;
        case ncpa::token_result::not_configured:
          // Not a guess, so it does not count towards the block: every
          // request fails the same way until the operator sets a token.
          failure = "no token is configured. Set 'token' under /settings/NCPA/server.";
          break;
        case ncpa::token_result::rejected:
          rate_limiter_.record_failure(remote);
          failure = (ncpa::form_has(args, "token") ? std::string("wrong token") : std::string("no token given")) + ".";
          break;
      }
    }
  }
  NSC_LOG_ERROR("NCPA: rejected request from " + remote + ": " + failure);
  answer_json(response, ncpa::error_body(blocked ? kBlocked : kBadCredentials));
  return false;
}

void ncpa_controller::api(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  // Caught here rather than left to the HTTP layer: what it does with an
  // escaping exception goes through an error sink only the WEB server
  // installs, so without the WEB server it would be a bare 500 with nothing
  // in the log. The caller gets NCPA's error shape and no internals.
  try {
    handle(request, what, response);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("NCPA: request from " + request.getRemoteIp() + " failed: " + utf8::utf8_from_native(e.what()));
    answer_json(response, ncpa::error_body("Internal error, see the agent log."));
  } catch (...) {
    NSC_LOG_ERROR("NCPA: request from " + request.getRemoteIp() + " failed with an unknown exception");
    answer_json(response, ncpa::error_body("Internal error, see the agent log."));
  }
}

void ncpa_controller::handle(Mongoose::Request &request, const boost::smatch &what, Mongoose::StreamResponse &response) {
  // Query string first, then a form body: NCPA reads both (Flask's
  // request.values), and the first value of a key wins.
  ncpa::form_vector args = request.getVariablesVector();
  if (request.getMethod() == "POST") {
    const std::string content_type = boost::algorithm::to_lower_copy(request.readHeader("Content-Type"));
    if (content_type.empty() || boost::algorithm::starts_with(content_type, "application/x-www-form-urlencoded")) {
      const ncpa::form_vector body = Mongoose::Request::parseVariables(request.getData());
      args.insert(args.end(), body.begin(), body.end());
    }
  }

  if (!authenticate(request, args, response)) return;

  const std::vector<std::string> path = ncpa::split_path(what.size() > 1 ? what.str(1) : std::string());
  const bool check_mode = ncpa::is_truthy(ncpa::form_value(args, "check"));
  const std::string full_path = join_path(path);

  if (path.empty()) {
    // The root walk. Only the plugins node exists so far; the built-in tree
    // (cpu, memory, disk, ...) is filled in by the following phases.
    if (check_mode) {
      answer_json(response, ncpa::check_body(3, "UNKNOWN: Unable to run check on node without check method. Requested 'root' node."));
      return;
    }
    answer_json(response, "{\"root\":" + ncpa::list_body("plugins", exposed_plugins()) + "}");
    return;
  }

  const std::string node = boost::algorithm::to_lower_copy(path.front());
  if (node == "plugins") {
    plugins(path, args, check_mode, full_path, request.getRemoteIp(), response);
    return;
  }

  answer_json(response, check_mode ? ncpa::missing_node_check_body("node", path.front()) : ncpa::missing_node_body(full_path, "node", path.front()));
}

std::vector<std::string> ncpa_controller::exposed_plugins() const {
  std::vector<std::string> names;
  for (const ncpa_sources::query_info &q : sources_->list_queries()) {
    if (config_.plugins.allows(q.name, q.module)) names.push_back(q.name);
  }
  std::sort(names.begin(), names.end());
  names.erase(std::unique(names.begin(), names.end()), names.end());
  return names;
}

void ncpa_controller::plugins(const std::vector<std::string> &path, const ncpa::form_vector &args, const bool check_mode, const std::string &full_path,
                              const std::string &remote, Mongoose::StreamResponse &response) {
  if (path.size() == 1) {
    // Listing the plugin directory is a walk even with check=1 set, as in NCPA.
    answer_json(response, ncpa::list_body("plugins", exposed_plugins()));
    return;
  }

  const std::string &name = path[1];
  // A query that is not exposed answers exactly like one that does not exist,
  // so the plugins node cannot be used to map what else is registered.
  ncpa_sources::query_info info;
  if (!sources_->describe_query(name, info) || !config_.plugins.allows(name, info.module)) {
    NSC_DEBUG_MSG("NCPA: " + remote + " asked for plugin '" + name + "', which is not registered or not exposed by 'plugins = " + config_.plugins.to_string() +
                  "'.");
    answer_json(response, check_mode ? ncpa::missing_node_check_body("plugin", name) : ncpa::missing_node_body(full_path, "plugin", name));
    return;
  }

  // The arguments are every `args=` value and then the path segments after
  // the name - NCPA's order - joined with spaces and split again (see
  // ncpa::split_args for why the second split is needed).
  std::vector<std::string> raw = ncpa::form_values(args, "args");
  raw.insert(raw.end(), path.begin() + 2, path.end());
  const std::vector<std::string> tokens = ncpa::split_args(boost::algorithm::join(raw, " "));
  const std::list<std::string> arguments(tokens.begin(), tokens.end());

  if (!arguments.empty() && !config_.allow_arguments) {
    NSC_LOG_WARNING("NCPA: refused plugin '" + name + "' from " + remote +
                    ": the request carried arguments and 'allow arguments' is false under /settings/NCPA/server.");
    answer_json(response, ncpa::check_body(3, "UNKNOWN: Arguments are not allowed (set 'allow arguments = true' under /settings/NCPA/server)."));
    return;
  }

  std::string message, perf;
  const int code = sources_->run_query(name, arguments, message, perf);
  answer_json(response, ncpa::check_body(code, ncpa::nagios_output(message, perf)));
}
