// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "NCPAServer.h"

#include <boost/filesystem/operations.hpp>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>

#include "ncpa_controller.hpp"
#include "ncpa_sources.hpp"

namespace sh = nscapi::settings_helper;

namespace {
// The stock `tls version`, named because the registered default and the test
// for "did the operator touch it" have to agree (see WEBServer.cpp).
const char *const kDefaultTlsVersion = "1.2+";
const char *const kDefaultPort = "5693";

class ncpa_web_logger : public Mongoose::WebLogger {
  bool log_errors_;
  bool log_info_;
  bool log_debug_;

 public:
  ncpa_web_logger(const bool log_errors, const bool log_info, const bool log_debug) : log_errors_(log_errors), log_info_(log_info), log_debug_(log_debug) {}
  void log_error(const std::string &message) override {
    if (log_errors_) NSC_LOG_ERROR("NCPA: " + message);
  }
  void log_info(const std::string &message) override {
    if (log_info_) NSC_LOG_MESSAGE("NCPA: " + message);
  }
  void log_debug(const std::string &message) override {
    if (log_debug_) NSC_DEBUG_MSG("NCPA: " + message);
  }
};
}  // namespace

NCPAServer::NCPAServer() = default;
NCPAServer::~NCPAServer() {
  try {
    stop_server();
  } catch (...) {
    // A destructor must not throw; unloadModule has normally stopped it already.
  }
}

void NCPAServer::stop_server() {
  if (server_) {
    server_->stop();
    server_.reset();
  }
}

bool NCPAServer::loadModuleEx(std::string alias, const NSCAPI::moduleLoadMode mode) {
  // A settings reload re-enters here on the live module. The listener owns a
  // controller built from the previous settings, so it is stopped and a new
  // one started below rather than reconfigured in place.
  try {
    stop_server();
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("NCPA: failed to stop the running listener: " + utf8::utf8_from_native(e.what()));
    return false;
  } catch (...) {
    NSC_LOG_ERROR("NCPA: failed to stop the running listener");
    return false;
  }

  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("NCPA", std::move(alias), "server");

  ncpa_config config;
  std::string port;
  std::string plugins;
  std::string certificate;
  std::string key;
  std::string tls_version;
  std::string allowed_ciphers;
  bool allow_insecure = false;
  int threads = 10;
  bool log_errors = true;
  bool log_info = false;
  bool log_debug = false;

  // clang-format off
  settings.alias().add_path_to_settings()
    ("NCPA server", "Section for the NCPA (NCPAServer) protocol: the Nagios NCPA HTTP API, polled with check_ncpa.py and the Nagios XI NCPA wizard.")
    ("log", "Log configuration", "Configure which messages from the NCPA listener are logged.")
  ;
  // clang-format on
  settings.alias()
      .add_key_to_settings()
      .add_string("port", sh::string_key(&port, kDefaultPort), "PORT NUMBER", "Port to listen on. 5693 is the port check_ncpa.py and the XI wizard default to.")
      .add_password("token", sh::string_key(&config.token, ""), "TOKEN",
                    "The NCPA token (NCPA's `community_string`), passed by check_ncpa.py as -t. There is no default: until one is set every request is "
                    "refused. Compared in constant time, and never logged.")
      .add_password("backup token", sh::string_key(&config.backup_token, ""), "BACKUP TOKEN",
                    "A second accepted token (NCPA's `backup_community_string`), so the token can be rotated without a window where pollers fail. "
                    "Empty (the default) accepts only `token`.")
      .add_bool("allow arguments", sh::bool_key(&config.allow_arguments, false), "ALLOW ARGUMENTS",
                "Whether a request to the plugins node may carry arguments - path segments after the query name (check_ncpa -a) or `args=` "
                "parameters. False (the default) runs only the commands the agent defines, exactly as the NRPE server's `allow arguments`; define an "
                "alias to give a check fixed arguments.")
      .add_string("plugins", sh::string_key(&plugins, "any"), "EXPOSED PLUGINS",
                  "Which queries the plugins node exposes: `any` (every registered query - checks, aliases and external scripts), `scripts` (only "
                  "the commands CheckExternalScripts registers) or a comma-separated list of query names. A query that is not exposed answers "
                  "exactly like one that does not exist.")
      .add_int("auth rate limit max failures", sh::int_key(&config.auth_max_failures, auth_rate_limiter::kDefaultMaxFailures), "AUTH RATE LIMIT (FAILURES)",
               "How many consecutive wrong tokens from one address block it. Default 10; 0 disables the limiter.")
      .add_int("auth rate limit block seconds", sh::int_key(&config.auth_block_seconds, auth_rate_limiter::kDefaultBlockSeconds),
               "AUTH RATE LIMIT (BLOCK SECONDS)",
               "How long a blocked address stays blocked. Default 60 s, doubling for an address that keeps guessing at machine speed, up to an "
               "hour - the same limiter the WEB server uses.")
      .add_int("threads", sh::int_key(&threads, 10), "WORKER THREADS",
               "How many requests are answered at the same time. A check blocks the thread answering it until it returns, so a slow external "
               "script only delays the polls behind it once all threads are busy. Default 10.")
      .add_bool("allow insecure", sh::bool_key(&allow_insecure, false), "ALLOW INSECURE (CLEARTEXT HTTP)",
                "When false (the default) the listener refuses to start without a TLS certificate rather than serve the token in clear. Set to true "
                "only behind a TLS-terminating proxy or on loopback. Note that check_ncpa.py always connects with https.");
  settings.alias()
      .add_key_to_settings()
      .add_string("certificate", sh::path_key(&certificate, "${certificate-path}/certificate.pem"), "TLS CERTIFICATE",
                  "The certificate the listener serves. The default is the same file the WEB server uses, so one certificate serves both; a default "
                  "one is generated when it is missing. check_ncpa.py only verifies it when run with -s.")
      .add_string("certificate key", sh::path_key(&key), "TLS PRIVATE KEY", "The private key for the certificate if it is not in the same file.")
      .add_string("tls version", sh::string_key(&tls_version, kDefaultTlsVersion), "TLS VERSION",
                  "Which TLS versions the listener negotiates, in the same vocabulary as the WEB server: an exact version (1.0, 1.1, 1.2, 1.3), a "
                  "trailing + for that version or later, or `any`. Honoured on builds using the beast web backend (all Linux packages).")
      .add_string("allowed ciphers", sh::string_key(&allowed_ciphers), "ALLOWED CIPHERS",
                  "OpenSSL cipher list the listener is restricted to. Empty (the default) leaves the library's own selection in place.");
  settings.alias()
      .add_key_to_settings("log")
      .add_bool("error", sh::bool_key(&log_errors, true), "LOG ERRORS", "Log errors from the HTTP listener.")
      .add_bool("info", sh::bool_key(&log_info, false), "LOG INFO", "Log informational messages from the HTTP listener.")
      .add_bool("debug", sh::bool_key(&log_debug, false), "LOG DEBUG", "Log debug messages from the HTTP listener.");
  settings.alias()
      .add_parent("/settings/default")
      .add_key_to_settings()
      .add_string("allowed hosts", sh::string_key(&config.allowed_hosts, "127.0.0.1"), "Allowed hosts",
                  "A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges.")
      .add_bool("cache allowed hosts", sh::bool_key(&config.cache_allowed_hosts, true), "Cache list of allowed hosts",
                "If host names (DNS entries) should be cached, improves speed and security somewhat but won't allow you to have dynamic IPs for your "
                "Nagios server.");

  settings.register_all();
  settings.notify();

  if (mode != NSCAPI::normalStart && mode != NSCAPI::reloadStart) return true;

  {
    std::string error;
    if (!ncpa::plugin_policy::parse(plugins, config.plugins, error)) {
      NSC_LOG_ERROR("NCPA: " + error + ". The NCPA listener has NOT been started.");
      return true;
    }
  }
  if (config.token.empty()) {
    // Started anyway, so the port answers and the log line below is what a
    // failing poller leads the operator to.
    NSC_LOG_ERROR("NCPA: no token is configured under /settings/NCPA/server: every request will be refused until one is set.");
  }

  std::list<std::string> errors;
  socket_helpers::validate_certificate(certificate, errors);
  NSC_LOG_ERROR_LISTS(errors);
  const bool cert_missing = !boost::filesystem::is_regular_file(certificate);

  try {
    Mongoose::WebLoggerPtr logger(new ncpa_web_logger(log_errors, log_info, log_debug));
    server_.reset(Mongoose::Server::make_server(logger));
    // An untouched `tls version` goes in as empty, for the reason WEBServer.cpp
    // gives: the mongoose backend would otherwise log on every start that it
    // ignores a setting nobody wrote.
    server_->setTlsOptions(tls_version == kDefaultTlsVersion ? std::string() : tls_version, allowed_ciphers);
    // Both backends serve TLS exactly when a certificate *loaded*, so the file
    // existing is not enough: a missing key or an unreadable file would leave
    // the listener on plain HTTP. What decides is whether setSsl() succeeded.
    const bool tls = !cert_missing && server_->setSsl(certificate, key);
    if (!tls) {
      if (!allow_insecure) {
        NSC_LOG_ERROR("NCPA: the certificate at '" + certificate +
                      "' (or its key) could not be loaded: refusing to start the NCPA listener in cleartext HTTP, which would send the token in clear. "
                      "Fix the certificate, or set 'allow insecure = true' under /settings/NCPA/server. The NCPA listener has NOT been started.");
        server_.reset();
        return true;
      }
      NSC_LOG_ERROR("NCPA: no usable certificate at '" + certificate + "' and 'allow insecure = true' is set: serving UNENCRYPTED HTTP on port " + port +
                    ". The token travels in clear.");
    }
    // Checks run on a pool, so one slow check (an external script near its
    // timeout) does not hold up every other poll and TLS handshake.
    server_->setWorkerThreads(static_cast<std::size_t>(threads < 1 ? 1 : threads));
    // No error sink is installed: it is process-global and the WEB server owns
    // it. The controller catches and logs its own failures.
    server_->registerController(new ncpa_controller(config, std::make_shared<ncpa_sources>(get_core(), get_id())));
    if (!server_->start("0.0.0.0:" + port)) {
      NSC_LOG_ERROR("NCPA: the NCPA listener has NOT been started on port " + port + " (see the error above). Fix the configuration and reload.");
      server_.reset();
      return true;
    }
    NSC_DEBUG_MSG("NCPA: listening on port " + port + (tls ? " (https" : " (http") + ", plugins = " + config.plugins.to_string() + ")");
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("NCPA: the listener failed to start (the module stays loaded; fix the configuration and reload): " + utf8::utf8_from_native(e.what()));
    server_.reset();
  } catch (...) {
    NSC_LOG_ERROR("NCPA: the listener failed to start (the module stays loaded; fix the configuration and reload)");
    server_.reset();
  }
  return true;
}

void NCPAServer::prepareShutdown() {
  // Stop accepting and drain the request threads while every other module is
  // still loaded, so a query in flight can finish.
  try {
    if (server_) server_->stop();
  } catch (...) {
    NSC_LOG_ERROR_EX("prepare_shutdown");
  }
}

bool NCPAServer::unloadModule() {
  try {
    stop_server();
  } catch (...) {
    NSC_LOG_ERROR_EX("unload");
    return false;
  }
  return true;
}
