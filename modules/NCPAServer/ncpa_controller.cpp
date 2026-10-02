// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ncpa_controller.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <list>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
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
  allowed_hosts_.cached = config_.cache_allowed_hosts;
  allowed_hosts_.set_source(config_.allowed_hosts);
  std::list<std::string> errors;
  allowed_hosts_.refresh(errors);
  NSC_LOG_ERROR_LISTS(errors);
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
  const std::string &remote = request.getRemoteIp();
  std::list<std::string> errors;
  bool allowed = false;
  try {
    allowed = allowed_hosts_.is_allowed(boost::asio::ip::make_address(remote), errors);
  } catch (const std::exception &e) {
    errors.push_back(std::string("unparsable peer address: ") + e.what());
  }
  if (!allowed) {
    NSC_LOG_ERROR("NCPA: rejected connection from " + remote + (errors.empty() ? std::string() : " (" + str::utils::joinEx(errors, ", ") + ")") +
                  ": not in 'allowed hosts'.");
    // A plain 403, as NCPA answers a host outside its allow-list. check_ncpa
    // reports it as UNKNOWN with the HTTP status.
    response.setCodeForbidden("403 Your host is not allowed");
    return false;
  }

  if (rate_limiter_.is_blocked(remote)) {
    NSC_LOG_ERROR("NCPA: rejected request from " + remote + ": blocked after repeated failed tokens.");
    answer_json(response, ncpa::error_body(kBlocked));
    return false;
  }

  switch (ncpa::check_token(ncpa::form_value(args, "token"), config_.token, config_.backup_token)) {
    case ncpa::token_result::accepted:
      rate_limiter_.record_success(remote);
      return true;
    case ncpa::token_result::not_configured:
      // Not a guess, so it does not count towards the block: every request
      // fails the same way until the operator sets a token.
      NSC_LOG_ERROR("NCPA: rejected request from " + remote + ": no token is configured. Set 'token' under /settings/NCPA/server.");
      break;
    case ncpa::token_result::rejected:
      rate_limiter_.record_failure(remote);
      NSC_LOG_ERROR("NCPA: rejected request from " + remote + ": " +
                    (ncpa::form_has(args, "token") ? std::string("wrong token") : std::string("no token given")) + ".");
      break;
  }
  answer_json(response, ncpa::error_body(kBadCredentials));
  return false;
}

void ncpa_controller::api(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  // Query string first, then a form body: NCPA reads both (Flask's
  // request.values), and the first value of a key wins.
  ncpa::form_vector args = request.getVariablesVector();
  if (request.getMethod() == "POST") {
    const std::string content_type = boost::algorithm::to_lower_copy(request.readHeader("Content-Type"));
    if (content_type.empty() || boost::algorithm::starts_with(content_type, "application/x-www-form-urlencoded")) {
      const ncpa::form_vector body = ncpa::parse_form(request.getData());
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
    if (config_.plugins.allows(q.name, q.owner)) names.push_back(q.name);
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
  if (!sources_->describe_query(name, info) || !config_.plugins.allows(name, info.owner)) {
    NSC_DEBUG_MSG("NCPA: " + remote + " asked for plugin '" + name + "', which is not registered or not exposed by 'plugins = " + config_.plugins.to_string() +
                  "'.");
    answer_json(response, check_mode ? ncpa::missing_node_check_body("plugin", name) : ncpa::missing_node_body(full_path, "plugin", name));
    return;
  }

  // The arguments are the path segments after the name and every `args=`
  // value, joined with spaces and split again the way a shell would - what
  // NCPA itself does. It matters for check_ncpa.py: it tokenises -a with a
  // non-POSIX shlex, which keeps the quotes, so -a '"filter=load > 80"'
  // arrives as the one segment `"filter=load > 80"` and only this second split
  // removes them.
  std::vector<std::string> raw(path.begin() + 2, path.end());
  for (const std::string &value : ncpa::form_values(args, "args")) raw.push_back(value);
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
