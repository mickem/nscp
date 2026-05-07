/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "WEBServer.h"

#include <boost/filesystem/operations.hpp>
#include <boost/json.hpp>
#include <boost/program_options.hpp>
#include <cmath>
#include <cstdint>
#include <limits>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/nscapi_common_options.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/settings_functions.hpp>
#include <nscapi/settings/helper.hpp>
#include <str/format.hpp>
#include <str/xtos.hpp>
#include <utility>

#include "api_controller.hpp"
#include "error_handler.hpp"
#include "info_controller.hpp"
#include "legacy_command_controller.hpp"
#include "legacy_controller.hpp"
#include "log_controller.hpp"
#include "login_controller.hpp"
#include "metrics_controller.hpp"
#include "modules_controller.hpp"
#include "openmetrics_controller.hpp"
#include "password_hash.hpp"
#include "query_controller.hpp"
#include "scripts_controller.hpp"
#include "settings_controller.hpp"
#include "static_controller.hpp"
#include "token_store.hpp"
#include "web_cli_handler.hpp"

namespace json = boost::json;

namespace sh = nscapi::settings_helper;

using namespace std;
using namespace Mongoose;

class WEBServerLogger : public WebLogger {
  bool log_errors_;
  bool log_info_;
  bool log_debug_;

 public:
  WEBServerLogger(const bool log_errors, bool log_info, bool log_debug) : log_errors_(log_errors), log_info_(log_info), log_debug_(log_debug) {}
  void log_error(const std::string &message) override {
    if (log_errors_) {
      NSC_LOG_ERROR(message);
    }
  }
  void log_info(const std::string &message) override {
    if (log_info_) {
      NSC_LOG_MESSAGE(message);
    }
  }
  void log_debug(const std::string &message) override {
    if (log_debug_) {
      NSC_DEBUG_MSG(message);
    }
  }
};

WEBServer::WEBServer() : simple_plugin(), session(new session_manager_interface()), last_log_index(0) {}
WEBServer::~WEBServer() = default;

bool WEBServer::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  log_handler.reset(new error_handler());
  client.reset(new client::cli_client(boost::make_shared<web_cli_handler>(log_handler, get_core(), get_id())));

  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("WEB", std::move(alias), "server");

  std::string port;
  std::string certificate;
  std::string key;
  std::string admin_password;
  int threads;
  bool log_errors = true;
  bool log_info = false;
  bool log_debug = false;

  role_map roles;

  std::string role_path = settings.alias().get_settings_path("roles");
  std::string user_path = settings.alias().get_settings_path("users");

  users_.set_path(settings.alias().get_settings_path("users"));

  // clang-format off
  settings.alias().add_path_to_settings()
    ("Web server", "Section for WEB (WEBServer.dll) (check_WEB) protocol options.")
    ("log", "Log configuration", "Configure which messages from the web server are logged.")
    ("users", sh::fun_values_path([this] (auto key, auto value) { this->add_user(key, value); }),
    "Web server users", "Users which can access the REST API",
    "REST USER", "")
    ("roles", sh::string_map_path(&roles)
    , "Web server roles", "A list of roles and with coma separated list of access rights.")
  ;
  // clang-format on
  settings.alias()
      .add_key_to_settings()
      .add_string("port", sh::string_key(&port, "8443"), "Server port", "Port to use for WEB server.")
      .add_int("threads", sh::int_key(&threads, 10), "Server threads", "The number of threads in the sever response pool.")
      .add_bool(
          "allow anonymous access",
          nscapi::settings_helper::bool_fun_key([this](auto value) { this->session->set_allow_anonymous(value); }, false),
          "ALLOW ANONYMOUS ACCESS",
          "When false (the default) any role named `anonymous` registered via /settings/WEB/server/roles is ignored and the WEB server never answers an unauthenticated request. Set to true only if you intentionally want to expose endpoints (via the `anonymous` role grants) without authentication.");
  settings.alias()
      .add_key_to_settings()
      .add_string("certificate", sh::string_key(&certificate, "${certificate-path}/certificate.pem"), "TLS Certificate",
                  "Ssl certificate to use for the ssl server")
      .add_string("certificate key", sh::string_key(&key), "TLS private key", "The private key for the certificate if not in the same file");
  settings.alias()
      .add_key_to_settings("log")
      .add_bool("error", sh::bool_key(&log_errors, true), "Log errors", "Enable logging of errors from the web server.")
      .add_bool("info", sh::bool_key(&log_info, false), "Log info", "Enable logging of info messages from the web server.")
      .add_bool("debug", sh::bool_key(&log_debug, false), "Log debug", "Enable logging of debug messages from the web server.");

  settings.alias()
      .add_parent("/settings/default")
      .add_key_to_settings()
      .add_string("allowed hosts", nscapi::settings_helper::string_fun_key([this](auto value) { this->session->set_allowed_hosts(value); }, "127.0.0.1"),
                  "Allowed hosts", "A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges.")
      .add_string(
          "cache allowed hosts", nscapi::settings_helper::bool_fun_key([this](auto value) { this->session->set_allowed_hosts_cache(value); }, true),
          "Cache list of allowed hosts",
          "If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your Nagios server.")
      .add_password("password", nscapi::settings_helper::string_key(&admin_password), DEFAULT_PASSWORD_NAME, DEFAULT_PASSWORD_DESC);

  settings.register_all();
  settings.notify();
  certificate = get_core()->expand_path(certificate);
  key = get_core()->expand_path(key);

  users_.add_samples(nscapi::settings_proxy::create(get_id(), get_core()));

  // `legacy` deliberately does NOT include `console.exec` (which is
  // RCE-equivalent). Operators who want to expose the console must grant
  // `console.exec` explicitly, normally only under the `full` role (which is
  // already a wildcard).
  ensure_role(roles, settings, role_path, "legacy", "legacy,login.get", "legacy API");
  ensure_role(roles, settings, role_path, "full", "*", "Full access");
  ensure_role(roles, settings, role_path, "client", "public,info.get,info.get.version,queries.list,queries.get,queries.execute,login.get,modules.list",
              "read only");
  ensure_role(roles, settings, role_path, "view", "*", "Full access");

  ensure_user(settings, user_path, "admin", "full", admin_password, "Administrator");

  if (mode == NSCAPI::normalStart) {
    std::list<std::string> errors = session->boot();

    for (const web_server::user_config_instance &o : users_.get_object_list()) {
      session->add_user(o->get_alias(), o->role, o->password);
    }
    for (const role_map::value_type &v : roles) {
      session->add_grant(v.first, v.second);
    }

    socket_helpers::validate_certificate(certificate, errors);
    NSC_LOG_ERROR_LISTS(errors);
    std::string path = get_core()->expand_path("${web-path}");
    if (!boost::filesystem::is_directory(path)) {
      const std::string fallback = get_core()->expand_path("${exe-path}/web");
      if (boost::filesystem::is_directory(fallback)) {
        NSC_DEBUG_MSG("Web folder " + path + " not found, using " + fallback + " instead.");
        path = fallback;
      } else {
        NSC_LOG_ERROR("Failed to find web folder: " + path + " (also tried " + fallback + ")");
      }
    }
    // Silent HTTPS->HTTP downgrade is dangerous: a missing certificate flips
    // the agent from 8443 to 8080 with no operational signal beyond a single
    // log line. Keep the convenience auto-flip (so dev/test installs without
    // a cert do not fail to start) but make the consequences explicit on
    // every restart - session tokens travel in clear once HTTP is in use.
    const bool cert_missing = !boost::filesystem::is_regular_file(certificate);
    if (cert_missing && port == "8443") {
      NSC_LOG_ERROR(
          "WEB certificate not found at '" + certificate +
          "': falling back to HTTP on port 8080. Session tokens / Basic-auth credentials will be transmitted in clear. Do NOT use this configuration in "
          "production - either provide a valid certificate or front the agent with a TLS-terminating proxy.");
      port = "8080";
    }
    if (boost::ends_with(port, "s")) {
      port = port.substr(0, port.length() - 1);
    }

    if (!session->has_user("admin")) {
      session->add_user("admin", "full", admin_password);
    }

    WebLoggerPtr logger(new WEBServerLogger(log_errors, log_info, log_debug));
    server.reset(Server::make_server(logger));
    if (cert_missing) {
      NSC_LOG_ERROR("Certificate not found (disabling SSL): " + certificate);
    } else {
      NSC_DEBUG_MSG("Using certificate: " + certificate);
      server->setSsl(certificate, key);
    }

    server->registerController(new StaticController(session, path));

    server->registerController(new modules_controller(2, session, get_core(), get_id()));
    server->registerController(new query_controller(2, session, get_core(), get_id()));
    server->registerController(new scripts_controller(2, session, get_core(), get_id()));
    server->registerController(new log_controller(2, session, get_core(), get_id()));
    server->registerController(new info_controller(2, session, get_core(), get_id()));
    server->registerController(new settings_controller(2, session, get_core(), get_id()));
    server->registerController(new login_controller(2, session));
    server->registerController(new metrics_controller(2, session, get_core(), get_id()));
    server->registerController(new openmetrics_controller(2, session, get_core(), get_id()));

    server->registerController(new modules_controller(1, session, get_core(), get_id()));
    server->registerController(new query_controller(1, session, get_core(), get_id()));
    server->registerController(new scripts_controller(1, session, get_core(), get_id()));
    server->registerController(new log_controller(1, session, get_core(), get_id()));
    server->registerController(new info_controller(1, session, get_core(), get_id()));
    server->registerController(new settings_controller(1, session, get_core(), get_id()));
    server->registerController(new login_controller(1, session));

    server->registerController(new api_controller(session));

    server->registerController(new legacy_controller(session, get_core(), get_id(), client));
    server->registerController(new legacy_command_controller(session, get_core(), get_id(), client));

    try {
      server->start("0.0.0.0:" + port);
    } catch (const std::exception &e) {
      NSC_LOG_ERROR("Failed to start server: " + utf8::utf8_from_native(e.what()));
      return true;
    } catch (const std::string &e) {
      NSC_LOG_ERROR("Failed to start server: " + e);
      return true;
    }
    NSC_DEBUG_MSG("Loading webserver on port: " + port);
  }
  return true;
}

bool WEBServer::unloadModule() {
  try {
    if (server) {
      server->stop();
      server.reset();
    }
  } catch (...) {
    NSC_LOG_ERROR_EX("unload");
    return false;
  }
  return true;
}

void WEBServer::handleLogMessage(const PB::Log::LogEntry::Entry &message) {
  using namespace boost::posix_time;
  using namespace boost::gregorian;

  error_handler_interface::log_entry entry;
  entry.index = last_log_index++;
  entry.line = message.line();
  entry.file = message.file();
  entry.message = message.message();
  entry.date = to_simple_string(second_clock::local_time());

  switch (message.level()) {
    case PB::Log::LogEntry_Entry_Level_LOG_CRITICAL:
      entry.type = "critical";
      break;
    case PB::Log::LogEntry_Entry_Level_LOG_DEBUG:
      entry.type = "debug";
      break;
    case PB::Log::LogEntry_Entry_Level_LOG_ERROR:
      entry.type = "error";
      break;
    case PB::Log::LogEntry_Entry_Level_LOG_INFO:
      entry.type = "info";
      break;
    case PB::Log::LogEntry_Entry_Level_LOG_WARNING:
      entry.type = "warning";
      break;
    default:
      entry.type = "unknown";
  }
  session->add_log_message(message.level() == PB::Log::LogEntry_Entry_Level_LOG_CRITICAL || message.level() == PB::Log::LogEntry_Entry_Level_LOG_ERROR, entry);
}

bool WEBServer::commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage::Request &request,
                                PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &_request_message) {
  std::string command = request.command();
  if (command == "web" && request.arguments_size() > 0)
    command = request.arguments(0);
  else if (target_mode == NSCAPI::target_module && request.arguments_size() > 0)
    command = request.arguments(0);
  else if (command.empty() && target_mode == NSCAPI::target_module)
    command = "help";
  if (command == "install") return install_server(request, response);
  if (command == "add-user") return cli_add_user(request, response);
  if (command == "add-role")
    return cli_add_role(request, response);
  else if (command == "password")
    return password(request, response);
  else if (target_mode == NSCAPI::target_module) {
    nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp web [install|password|add-user|add-role] --help");
    return true;
  }
  return false;
}

bool WEBServer::cli_add_user(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  namespace po = boost::program_options;
  namespace pf = nscapi::protobuf::functions;
  po::variables_map vm;
  po::options_description desc;
  std::string user, password, role;

  desc.add_options()("help", "Show help.")

      ("user", po::value<std::string>(&user), "The username to login as")

          ("password", po::value<std::string>(&password), "The password to login with")

              ("role", po::value<std::string>(&role), "The role to grant to the user")

      ;

  try {
    nscapi::program_options::basic_command_line_parser cmd(request);
    cmd.options(desc);

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);

    if (vm.count("help")) {
      nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
      return true;
    }

    const std::string path = "/settings/WEB/server/users/" + user;

    pf::settings_query q(get_id());
    q.get(path, "password", "");
    q.get(path, "role", "");

    get_core()->settings_query(q.request(), q.response());
    if (!q.validate_response()) {
      nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
      return true;
    }
    // Track whether we actually have a fresh plaintext to display to the
    // operator. We never echo a stored hash (it's useless to copy) and we
    // never display an existing on-disk plaintext we didn't change either.
    const bool password_from_cli = vm.count("password") > 0;
    bool password_was_supplied = password_from_cli;
    for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
      if (val.matches(path, "password") && password.empty())
        password = val.get_string();
      else if (val.matches(path, "role") && role.empty())
        role = val.get_string();
    }

    std::stringstream result;
    std::string password_for_display;
    if (password.empty()) {
      result << "WARNING: No password specified using a generated password" << std::endl;
      password = token_store::generate_token(32);
      password_for_display = password;
    } else if (web_password::is_hashed(password)) {
      // Existing on-disk hash; nothing to migrate, nothing to show.
      password_for_display = "(unchanged)";
    } else if (password_was_supplied) {
      // Operator supplied plaintext on the CLI - show it back, then hash.
      password_for_display = password;
    } else {
      // Legacy plaintext sitting on disk - migrate to a hash. We don't echo
      // it back (the operator already has it; rotating it isn't this command's
      // job).
      password_for_display = "(migrated to hash)";
    }
    if (role.empty()) {
      result << "WARNING: No role specified using client" << std::endl;
      role = "client";
    }

    // Hash the per-user password before persisting. The /settings/default
    // password (shared with NRPE / NSCA / NSClient) is untouched - those
    // protocols still need the plaintext to compare on the wire.
    if (!web_password::is_hashed(password)) {
      const std::string hashed = web_password::hash_password(password);
      if (hashed.empty()) {
        nscapi::protobuf::functions::set_response_bad(*response, "Failed to hash password (RNG / KDF failure)");
        return true;
      }
      password = hashed;
    }

    nscapi::protobuf::functions::settings_query s(get_id());
    result << "User " << user << " authenticated by " << password_for_display << " as " << role << std::endl;
    s.set(path, "password", password);
    s.set(path, "role", role);
    s.save();
    get_core()->settings_query(s.request(), s.response());
    if (!s.validate_response()) {
      nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
      return true;
    }
    nscapi::protobuf::functions::set_response_good(*response, result.str());
    return true;
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
    return true;
  } catch (...) {
    nscapi::program_options::invalid_syntax(desc, request.command(), "Unknown exception", *response);
    return true;
  }
}

bool WEBServer::cli_add_role(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  namespace po = boost::program_options;
  namespace pf = nscapi::protobuf::functions;
  po::variables_map vm;
  po::options_description desc;
  std::string role, grant;

  desc.add_options()("help", "Show help.")

      ("role", po::value<std::string>(&role), "The role to update grants for")

          ("grant", po::value<std::string>(&grant), "The grants to give to the role")

      ;

  try {
    nscapi::program_options::basic_command_line_parser cmd(request);
    cmd.options(desc);

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);

    if (vm.count("help")) {
      nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
      return true;
    }

    if (role.empty()) {
      nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
      return true;
    }

    std::stringstream result;

    const std::string path = "/settings/WEB/server/roles";

    pf::settings_query q(get_id());
    q.get(path, role, "");

    get_core()->settings_query(q.request(), q.response());
    if (!q.validate_response()) {
      nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
      return true;
    }
    for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
      if (val.matches(path, role) && grant.empty()) {
        grant = val.get_string();
      }
    }

    nscapi::protobuf::functions::settings_query s(get_id());
    result << "Role " << role << std::endl;
    for (const std::string &g : str::utils::split<std::list<std::string> >(grant, ",")) {
      result << " " << g << std::endl;
    }
    s.set(path, role, grant);
    s.save();
    get_core()->settings_query(s.request(), s.response());
    if (!s.validate_response()) {
      nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
      return true;
    }
    nscapi::protobuf::functions::set_response_good(*response, result.str());
    return true;
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
    return true;
  } catch (...) {
    nscapi::program_options::invalid_syntax(desc, request.command(), "Unknown exception", *response);
    return true;
  }
}
bool WEBServer::install_server(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  namespace po = boost::program_options;
  namespace pf = nscapi::protobuf::functions;
  po::options_description desc;
  std::string allowed_hosts, cert, key, port, password;
  const std::string path = "/settings/WEB/server";

  pf::settings_query q(get_id());
  q.get("/settings/default", "allowed hosts", "127.0.0.1");
  q.get("/settings/default", "password", "");
  q.get(path, "certificate", "${certificate-path}/certificate.pem");
  q.get(path, "certificate key", "");
  q.get(path, "port", "8443");

  get_core()->settings_query(q.request(), q.response());
  if (!q.validate_response()) {
    nscapi::protobuf::functions::set_response_bad(*response, q.get_response_error());
    return true;
  }
  for (const pf::settings_query::key_values &val : q.get_query_key_response()) {
    if (val.matches("/settings/default", "allowed hosts"))
      allowed_hosts = val.get_string();
    else if (val.matches("/settings/default", "password"))
      password = val.get_string();
    else if (val.matches(path, "certificate"))
      cert = val.get_string();
    else if (val.matches(path, "certificate key"))
      key = val.get_string();
    else if (val.matches(path, "port"))
      port = val.get_string();
  }
  bool want_https = !cert.empty();

  // clang-format off
  desc.add_options()("help", "Show help.")
      ("allowed-hosts,h", po::value<std::string>(&allowed_hosts)->default_value(allowed_hosts), "Set which hosts are allowed to connect")
      ("certificate", po::value<std::string>(&cert)->default_value(cert), "Length of payload (has to be same as on the server)")
      ("certificate-key", po::value<std::string>(&key)->default_value(key), "Client certificate to use")
      ("port", po::value<std::string>(&port)->default_value(port), "Port to use")
      ("password", po::value<std::string>(&password)->default_value(password), "Password to use to authenticate (if none a generated password will be set)")
      ("https", boost::program_options::bool_switch(&want_https), "Enable https")
      ;
  // clang-format on

  try {
    po::variables_map vm;
    nscapi::program_options::basic_command_line_parser cmd(request);
    cmd.options(desc);

    po::parsed_options parsed = cmd.run();
    po::store(parsed, vm);
    po::notify(vm);

    if (vm.count("help")) {
      nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
      return true;
    }
    std::stringstream result;

    if (password.empty()) {
      result << "WARNING: No password specified using a generated password" << std::endl;
      password = token_store::generate_token(32);
    }

    nscapi::protobuf::functions::settings_query s(get_id());
    result << "Enabling WEB access from " << allowed_hosts << std::endl;
    s.set("/settings/default", "allowed hosts", allowed_hosts);
    s.set("/modules", "WEBServer", "enabled");
    boost::replace_all(port, "s", "");
    if (!want_https) {
      cert = "";
      key = "";
      result << "Point your browser to http://localhost:" << port << std::endl;
    } else {
      if (cert == key || key.empty()) {
        result << "Certificate & key: " << get_core()->expand_path(cert) << "." << std::endl;
      } else {
        result << "Certificate: " << get_core()->expand_path(cert) << std::endl;
        result << "Certificate key: " << get_core()->expand_path(key) << std::endl;
      }
      const auto certificate = get_core()->expand_path(cert);
      std::list<std::string> messages;
      socket_helpers::validate_certificate(certificate, messages);
      for (const auto &e : messages) {
        result << "Certificate validation: " << e << std::endl;
      }
      result << "Point your browser to -- https://localhost:" << port << std::endl;
    }
    s.set(path, "certificate", cert);
    s.set(path, "certificate key", key);

    result << "Login using this password " << password << std::endl;
    s.set("/settings/default", "password", password);
    s.set(path, "port", port);
    s.save();
    get_core()->settings_query(s.request(), s.response());
    if (!s.validate_response()) {
      nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
      return true;
    }
    nscapi::protobuf::functions::set_response_good(*response, result.str());
    return true;
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
    return true;
  } catch (...) {
    nscapi::program_options::invalid_syntax(desc, request.command(), "Unknown exception", *response);
    return true;
  }
}

bool WEBServer::password(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) {
  namespace po = boost::program_options;
  namespace pf = nscapi::protobuf::functions;
  po::variables_map vm;
  po::options_description desc;

  std::string password;
  bool display = false, setweb = false;

  desc.add_options()("help", "Show help.")

      ("set,s", po::value<std::string>(&password), "Set the new password")

          ("display,d", po::bool_switch(&display), "Display the current configured password")

              ("only-web", po::bool_switch(&setweb), "Set the password for WebServer only (if not specified the default password is used)")

      ;
  try {
    nscapi::program_options::basic_command_line_parser cmd(request);
    cmd.options(desc);

    po::parsed_options parsed = cmd.allow_unregistered().run();
    po::store(parsed, vm);
    po::notify(vm);

    if (vm.count("help")) {
      nscapi::protobuf::functions::set_response_good(*response, nscapi::program_options::help(desc));
      return true;
    }
  } catch (const std::exception &e) {
    nscapi::program_options::invalid_syntax(desc, request.command(), "Invalid command line: " + utf8::utf8_from_native(e.what()), *response);
    return true;
  }

  if (display) {
    sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
    settings.set_alias("WEB", "", "server");

    settings.alias()
        .add_parent("/settings/default")
        .add_key_to_settings()

        .add_password("password", sh::string_key(&password), "PASSWORD", "Password used to authenticate against server")

        ;

    settings.register_all();
    settings.notify();
    if (password.empty())
      nscapi::protobuf::functions::set_response_good(*response, "No password set you will not be able to login");
    else
      nscapi::protobuf::functions::set_response_good(*response, "Current password: " + password);
  } else if (!password.empty()) {
    nscapi::protobuf::functions::settings_query s(get_id());
    if (setweb) {
      s.set("/settings/default", "password", password);
      s.set("/settings/WEB/server", "password", "");
    } else {
      s.set("/settings/WEB/server", "password", password);
    }

    s.save();
    get_core()->settings_query(s.request(), s.response());
    if (!s.validate_response()) {
      nscapi::protobuf::functions::set_response_bad(*response, s.get_response_error());
      return true;
    }
    nscapi::protobuf::functions::set_response_good(*response, "Password updated successfully, please restart nsclient++ for changes to affect.");
  } else {
    nscapi::protobuf::functions::set_response_bad(*response, nscapi::program_options::help(desc));
  }
  return true;
}

namespace {
json::value gauge_to_json(double v) {
  if (std::trunc(v) == v && v >= static_cast<double>(std::numeric_limits<std::int64_t>::min()) &&
      v <= static_cast<double>(std::numeric_limits<std::int64_t>::max())) {
    return json::value(static_cast<std::int64_t>(v));
  }
  return json::value(v);
}
}  // namespace

void build_metrics(json::object &metrics, json::object &metrics_list, std::list<std::string> &openmetrics, const std::string &trail,
                   const std::string &opentrail, const PB::Metrics::MetricsBundle &b) {
  json::object node;
  for (const PB::Metrics::MetricsBundle &b2 : b.children()) {
    build_metrics(node, metrics_list, openmetrics, trail + "." + b2.key(), opentrail + "_" + b2.key(), b2);
  }
  for (const PB::Metrics::Metric &v : b.value()) {
    if (v.has_gauge_value()) {
      node.insert(json::object::value_type(v.key(), gauge_to_json(v.gauge_value().value())));
      metrics_list.insert(json::object::value_type(trail + "." + v.key(), gauge_to_json(v.gauge_value().value())));
      openmetrics.push_back(opentrail + "_" + v.key() + " " + str::xtos(v.gauge_value().value()));
    } else if (v.has_string_value()) {
      node.insert(json::object::value_type(v.key(), v.string_value().value()));
      metrics_list.insert(json::object::value_type(trail + "." + v.key(), v.string_value().value()));
    }
  }
  metrics.insert(json::object::value_type(b.key(), node));
}
void WEBServer::submitMetrics(const PB::Metrics::MetricsMessage &response) const {
  json::object metrics, metrics_list;
  std::list<std::string> openmetrics;
  for (const PB::Metrics::MetricsMessage::Response &p : response.payload()) {
    for (const PB::Metrics::MetricsBundle &b : p.bundles()) {
      build_metrics(metrics, metrics_list, openmetrics, b.key(), b.key(), b);
    }
  }
  session->set_metrics(json::serialize(metrics), json::serialize(metrics_list), openmetrics);
  client->push_metrics(response);
}

void WEBServer::add_user(const std::string &key, const std::string &arg) {
  try {
    users_.add(nscapi::settings_proxy::create(get_id(), get_core()), key, arg);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("Failed to add user: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to add user: " + key);
  }
}

void WEBServer::ensure_role(role_map &roles, const nscapi::settings_helper::settings_registry &settings, const std::string &role_path, const std::string &role,
                            const std::string &value, const std::string &reason) {
  if (roles.find(role) == roles.end()) {
    roles[role] = value;
    settings.register_key_string(role_path, role, "Role for " + reason, "Default role for " + reason, value);
    settings.set_static_key(role_path, role, value);
  }
}

void WEBServer::ensure_user(const nscapi::settings_helper::settings_registry &settings, const std::string &path, const std::string &user,
                            const std::string &role, const std::string &password, const std::string &reason) {
  if (!session->has_user(user)) {
    session->add_user(user, role, password);
    const std::string the_path = path + "/" + user;
    // Per-user passwords on disk are stored hashed. The default password
    // under /settings/default/password (shared with NRPE / NSCA / NSClient)
    // is left alone; only this per-user slot is migrated.
    std::string stored = password;
    if (!stored.empty() && !web_password::is_hashed(stored)) {
      const std::string hashed = web_password::hash_password(stored);
      if (!hashed.empty()) {
        stored = hashed;
      }
    }
    settings.register_key_password(the_path, "password", "Password for " + reason, "Password name for" + reason, stored);
    settings.set_static_key(the_path, "password", stored);
    settings.register_key_string(the_path, "role", "Role for " + reason, "Role name for" + reason, role);
    settings.set_static_key(the_path, "role", role);
  }
}
