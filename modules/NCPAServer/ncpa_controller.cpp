// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "ncpa_controller.hpp"

#include <algorithm>
#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/json.hpp>
#include <boost/thread/thread.hpp>
#include <cctype>
#include <nscapi/macros.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <utility>

namespace json = boost::json;

namespace ncpa {

namespace {

// Compare two secrets without letting the running time say how much of the
// candidate was right. Always walks the longer of the two.
bool constant_time_equals(const std::string &a, const std::string &b) {
  const std::size_t len = std::max(a.size(), b.size());
  unsigned char diff = a.size() == b.size() ? 0 : 1;
  for (std::size_t i = 0; i < len; ++i) {
    const unsigned char ca = i < a.size() ? static_cast<unsigned char>(a[i]) : 0;
    const unsigned char cb = i < b.size() ? static_cast<unsigned char>(b[i]) : 0;
    diff |= static_cast<unsigned char>(ca ^ cb);
  }
  return diff == 0;
}

// The NCPA error body. It is answered with HTTP 200 on purpose: check_ncpa.py
// turns any non-2xx status into "UNKNOWN: An error occurred connecting to API",
// which tells an operator nothing, whereas this body reaches Nagios as
// "CRITICAL: <message>".
void write_error(Mongoose::StreamResponse &response, const std::string &message) {
  json::object body;
  body["error"] = message;
  response.setCodeOk();
  response.setHeader("Content-Type", "application/json");
  response.append(json::serialize(body));
}

int hex_value(const char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// The largest `sleep` the server will honour. check_ncpa's own timeout defaults
// to 58 seconds, and a longer sleep would only hold an HTTP worker until the
// client has already given up.
const long kMaxSleepSeconds = 30;

}  // namespace

std::string url_decode(const std::string &value) {
  std::string out;
  out.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    const char c = value[i];
    if (c == '%' && i + 2 < value.size()) {
      const int hi = hex_value(value[i + 1]);
      const int lo = hex_value(value[i + 2]);
      if (hi >= 0 && lo >= 0) {
        out.push_back(static_cast<char>((hi << 4) | lo));
        i += 2;
        continue;
      }
    }
    // A literal "+" is a space only in a query string, never in a path
    // segment, and these are path segments.
    out.push_back(c);
  }
  return out;
}

bool session::token_matches(const std::string &candidate) const {
  const boost::mutex::scoped_lock lock(mutex_);
  if (token_.empty()) return false;
  // Both comparisons always run: returning early on the primary match would
  // make a backup-token request measurably slower than a primary-token one.
  const bool primary = constant_time_equals(candidate, token_);
  const bool backup = !backup_token_.empty() && constant_time_equals(candidate, backup_token_);
  return primary || backup;
}

bool session::has_token() const {
  const boost::mutex::scoped_lock lock(mutex_);
  return !token_.empty();
}

void session::set_token(const std::string &token) {
  const boost::mutex::scoped_lock lock(mutex_);
  token_ = token;
}

void session::set_backup_token(const std::string &token) {
  const boost::mutex::scoped_lock lock(mutex_);
  backup_token_ = token;
}

void session::set_allowed_hosts(const std::string &hosts) { allowed_hosts_.set_source(hosts); }

void session::set_allowed_hosts_cache(const bool cached) { allowed_hosts_.cached = cached; }

bool session::is_allowed(const std::string &ip, std::list<std::string> &errors) const {
  try {
    return allowed_hosts_.is_allowed(boost::asio::ip::make_address(ip), errors);
  } catch (const std::exception &e) {
    // An address the resolver cannot parse is not one we can match against the
    // allow-list, so it is not allowed.
    errors.emplace_back(std::string("could not parse the peer address: ") + e.what());
    return false;
  }
}

std::list<std::string> session::boot() {
  std::list<std::string> errors;
  allowed_hosts_.refresh(errors);
  return errors;
}

void session::set_allow_arguments(const bool allow) {
  const boost::mutex::scoped_lock lock(mutex_);
  allow_arguments_ = allow;
}

bool session::allow_arguments() const {
  const boost::mutex::scoped_lock lock(mutex_);
  return allow_arguments_;
}

void session::set_exposed_plugins(const std::string &spec) {
  const boost::mutex::scoped_lock lock(mutex_);
  exposed_.clear();
  scripts_only_ = false;
  const std::string trimmed = boost::algorithm::trim_copy(spec);
  if (trimmed.empty() || boost::iequals(trimmed, "any")) return;
  if (boost::iequals(trimmed, "scripts")) {
    scripts_only_ = true;
    return;
  }
  for (std::string &name : str::utils::split_lst(trimmed, std::string(","))) {
    boost::algorithm::trim(name);
    if (!name.empty()) exposed_.push_back(name);
  }
}

bool session::exposes(const std::string &name) const {
  const boost::mutex::scoped_lock lock(mutex_);
  // `scripts` is not decided here: which queries an external-scripts module
  // registered is something only the core knows, so the dispatcher answers it
  // and never asks this. Reaching it anyway would be a bug, and exposing
  // everything would be the wrong way to fail.
  if (scripts_only_) return false;
  if (exposed_.empty()) return true;
  return std::find(exposed_.begin(), exposed_.end(), name) != exposed_.end();
}

bool session::exposes_everything() const {
  const boost::mutex::scoped_lock lock(mutex_);
  return exposed_.empty() && !scripts_only_;
}

bool session::scripts_only() const {
  const boost::mutex::scoped_lock lock(mutex_);
  return scripts_only_;
}

std::vector<std::string> session::plugin_allow_list() const {
  const boost::mutex::scoped_lock lock(mutex_);
  return exposed_;
}

void session::set_default_units(const std::string &units) {
  const boost::mutex::scoped_lock lock(mutex_);
  default_units_ = units;
}

std::string session::default_units() const {
  const boost::mutex::scoped_lock lock(mutex_);
  return default_units_;
}

void session::set_expose_version(const bool expose) {
  const boost::mutex::scoped_lock lock(mutex_);
  expose_version_ = expose;
}

bool session::expose_version() const {
  const boost::mutex::scoped_lock lock(mutex_);
  return expose_version_;
}

controller::controller(session_ptr session, metrics_snapshot *snapshot, query_dispatcher *dispatcher, delta_store *deltas, tree_options options)
    : RegexpController("/api"), session_(std::move(session)), snapshot_(snapshot), dispatcher_(dispatcher), deltas_(deltas), options_(std::move(options)) {
  // check_ncpa.py only ever sends GET, but the agent accepts POST as well and
  // some wizard-generated commands use it.
  addRoute("GET", "/?$", this, &controller::handle_root);
  addRoute("POST", "/?$", this, &controller::handle_root);
  addRoute("GET", "/(.+)$", this, &controller::handle_path);
  addRoute("POST", "/(.+)$", this, &controller::handle_path);
}

std::string controller::parameter(const Mongoose::Request &request, const std::string &key, const std::string &fallback) {
  const std::string from_query = request.get(key, std::string());
  if (!from_query.empty()) return from_query;
  // A form-encoded POST body carries the same parameters in the same syntax,
  // so the query-string parser reads it unchanged.
  const std::string &body = request.getData();
  if (body.empty()) return fallback;
  const std::string content_type = request.readHeader("Content-Type");
  if (!content_type.empty() && content_type.find("application/x-www-form-urlencoded") == std::string::npos) return fallback;
  const Mongoose::Request form("", false, "POST", "", body, Mongoose::Request::headers_type(), "");
  const std::string from_body = form.get(key, std::string());
  return from_body.empty() ? fallback : from_body;
}

bool controller::has_parameter(const Mongoose::Request &request, const std::string &key) { return !parameter(request, key).empty(); }

bool controller::flag(const Mongoose::Request &request, const std::string &key) {
  const std::string value = parameter(request, key);
  if (value.empty()) return false;
  // NCPA reads these in Python, where any non-empty string is true - so the
  // real agent treats `delta=False` as "delta on". check_ncpa.py never sends
  // that (optparse leaves the switch at None and the plugin drops it), so
  // honouring the obvious negations costs no compatibility and stops a
  // hand-written URL from meaning the opposite of what it says.
  return !(boost::iequals(value, "false") || boost::iequals(value, "none") || value == "0");
}

bool controller::authenticate(Mongoose::Request &request, Mongoose::StreamResponse &response) const {
  const std::string remote_ip = request.getRemoteIp();

  std::list<std::string> errors;
  if (!session_->is_allowed(remote_ip, errors)) {
    NSC_LOG_ERROR("Rejected NCPA connection from " + remote_ip + ": " + str::utils::joinEx(errors, ", "));
    // A network-policy refusal, not a check result: answering the NCPA error
    // body here would report a CRITICAL check to Nagios and hide the fact that
    // the agent never even looked at the request.
    response.setCodeForbidden("403 Forbidden");
    return false;
  }

  if (session_->rate_limiter().is_blocked(remote_ip)) {
    response.setCodeForbidden("403 Forbidden");
    return false;
  }

  if (!session_->has_token()) {
    NSC_LOG_ERROR("Refused an NCPA request from " + remote_ip + ": no token is configured. Set 'token' under /settings/NCPA/server.");
    write_error(response, "Incorrect credentials given.");
    return false;
  }

  const std::string token = parameter(request, "token");
  if (!session_->token_matches(token)) {
    session_->rate_limiter().record_failure(remote_ip);
    // Never the token itself, not even a prefix: this line goes to a log file
    // operators paste into issues.
    NSC_LOG_ERROR("Rejected NCPA request from " + remote_ip + ": invalid token.");
    write_error(response, "Incorrect credentials given.");
    return false;
  }
  session_->rate_limiter().record_success(remote_ip);
  return true;
}

void controller::handle_root(Mongoose::Request &request, boost::smatch & /*what*/, Mongoose::StreamResponse &response) { serve("", request, response); }

void controller::handle_path(Mongoose::Request &request, boost::smatch &what, Mongoose::StreamResponse &response) {
  if (!RegexpController::validate_arguments(1, what, response)) return;
  serve(what.str(1), request, response);
}

void controller::serve(const std::string &accessor, Mongoose::Request &request, Mongoose::StreamResponse &response) {
  if (!authenticate(request, response)) return;

  try {
    walk_context ctx;
    ctx.accessor = accessor;
    ctx.remote_addr = request.getRemoteIp();
    ctx.deltas = deltas_;
    ctx.opts.warning = parameter(request, "warning");
    ctx.opts.critical = parameter(request, "critical");
    ctx.opts.unit = parameter(request, "unit");
    ctx.opts.aggregate = parameter(request, "aggregate");
    ctx.opts.title = parameter(request, "title");
    ctx.opts.perfdata_label = parameter(request, "perfdata_label");
    ctx.opts.delta = flag(request, "delta");
    // `units` falls back to the configured default, which is NCPA's
    // [general] default_units.
    ctx.opts.units = parameter(request, "units", session_->default_units());
    // Everything else on the query string is a filter for the `services` and
    // `processes` nodes: check_ncpa.py's `-q "service=sshd,status=running"`
    // lands here verbatim. The token is dropped so a secret never reaches a
    // node, and a repeated key is kept repeated - NCPA reads these as lists.
    for (const Mongoose::Request::arg_entry &entry : request.getVariablesVector()) {
      if (entry.first == "token") continue;
      ctx.extras.add(entry.first, entry.second);
    }

    // `sleep` lets a client ask for a fresh sample rather than whatever the 1 Hz
    // collector last published. Capped, because the sleep happens on an HTTP
    // worker.
    const std::string sleep = parameter(request, "sleep");
    if (!sleep.empty()) {
      const long seconds = str::stox<long>(sleep, 0);
      if (seconds > 0) boost::this_thread::sleep_for(boost::chrono::seconds(std::min(seconds, kMaxSleepSeconds)));
    }

    tree_options options = options_;
    options.expose_version = session_->expose_version();
    const node_ptr root = build_root(*snapshot_, options, dispatcher_);

    std::vector<std::string> path;
    for (const std::string &segment : split_accessor(accessor)) path.push_back(url_decode(segment));
    const node_ptr node = resolve(root, path, "/api/" + accessor);

    response.setHeader("Content-Type", "application/json");
    // The real agent sets this so the built-in web UI can poll a different
    // host's API from a browser; a wizard-generated dashboard relies on it.
    response.setHeader("Access-Control-Allow-Origin", "*");
    response.setCodeOk();

    if (flag(request, "check")) {
      const check_result result = node->run_check(ctx, render_options());
      json::object body;
      body["returncode"] = result.returncode;
      body["stdout"] = result.stdout_text;
      response.append(json::serialize(body));
      return;
    }

    // A plugin's document is the whole answer; every other node's is the value
    // of a `{"<name>": ...}` pair.
    if (node->answers_unwrapped()) {
      response.append(json::serialize(node->walk_body(ctx)));
    } else {
      response.append(json::serialize(node->walk(ctx)));
    }
  } catch (const std::exception &e) {
    NSC_LOG_ERROR(std::string("Failed to serve an NCPA request: ") + e.what());
    write_error(response, std::string("Failed to process request: ") + e.what());
  } catch (...) {
    NSC_LOG_ERROR("Failed to serve an NCPA request");
    write_error(response, "Failed to process request");
  }
}

}  // namespace ncpa
