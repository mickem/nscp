// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "NCPAServer.h"

#include <boost/algorithm/string/trim.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <net/socket/allowed_hosts.hpp>
#include <net/socket/socket_helpers.hpp>
#include <net/web_server_logger.hpp>
#include <net/web_server_tls.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <threads/guarded_thread.hpp>

#include "ncpa_controller.hpp"
#include "ncpa_sources.hpp"

namespace sh = nscapi::settings_helper;

namespace {
const char *const kDefaultPort = "5693";

}  // namespace

// `cache allowed hosts = false` asks for the host names in `allowed hosts` to
// be resolved again rather than once. The allow-list is checked as each
// connection is accepted, on the listener's I/O thread, where a DNS lookup per
// connection would stall every other connection behind it - so the lookups
// happen here instead, every kRefreshSeconds, and the accept check only ever
// reads the resolved list.
struct NCPAServer::host_refresher {
  static constexpr int kRefreshSeconds = 60;
  std::shared_ptr<socket_helpers::allowed_hosts_manager> hosts;
  std::mutex mutex;
  std::condition_variable cv;
  bool stop = false;
  std::shared_ptr<boost::thread> thread;

  void run() {
    std::unique_lock<std::mutex> lock(mutex);
    while (!cv.wait_for(lock, std::chrono::seconds(kRefreshSeconds), [this] { return stop; })) {
      lock.unlock();
      std::list<std::string> errors;
      hosts->refresh(errors);
      NSC_LOG_ERROR_LISTS(errors);
      lock.lock();
    }
  }
  void halt() {
    {
      const std::lock_guard<std::mutex> lock(mutex);
      stop = true;
    }
    cv.notify_all();
    if (thread && thread->joinable()) thread->join();
  }
};

NCPAServer::NCPAServer() = default;
NCPAServer::~NCPAServer() {
  try {
    stop_server();
  } catch (...) {
    // A destructor must not throw; unloadModule has normally stopped it already.
  }
}

void NCPAServer::stop_server() {
  // Safe from any thread, a request thread of this very server included: the
  // last reference is then dropped on a thread of its own.
  Mongoose::stop_and_release(server_, NSC_THREAD_REPORTER);
  if (refresher_) {
    refresher_->halt();
    refresher_.reset();
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
  std::string allowed_hosts;
  bool cache_allowed_hosts = true;
  std::string bind_to;
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
                "When false (the default) the listener refuses to start unless its TLS certificate loads, rather than serve the token in clear. "
                "A missing default certificate is generated, so it never counts as missing: to serve plain HTTP - behind a TLS-terminating "
                "proxy, or on loopback - set `certificate` to empty as well as this to true. Note that check_ncpa.py always connects with https.");
  settings.alias()
      .add_key_to_settings()
      .add_string("certificate", sh::path_key(&certificate, "${certificate-path}/certificate.pem"), "TLS CERTIFICATE",
                  "The certificate the listener serves. The default is the same file the WEB server uses, so one certificate serves both; a default "
                  "one is generated when it is missing. check_ncpa.py only verifies it when run with -s.")
      .add_string("certificate key", sh::path_key(&key), "TLS PRIVATE KEY", "The private key for the certificate if it is not in the same file.")
      .add_string("tls version", sh::string_key(&tls_version, net::kDefaultWebTlsVersion), "TLS VERSION",
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
      // Same key, title and text as the other servers register under
      // /settings/default: they share one entry in the reference.
      .add_string("bind to", sh::string_key(&bind_to), "BIND TO ADDRESS",
                  "Allows you to bind server to a specific local address. This has to be a dotted ip address not a host name. Leaving this blank will bind "
                  "to all available IP addresses.")
      .add_string("allowed hosts", sh::string_key(&allowed_hosts, "127.0.0.1"), "Allowed hosts",
                  "A comma separated list of allowed hosts. You can use netmasks (/ syntax) or * to create ranges.")
      .add_bool("cache allowed hosts", sh::bool_key(&cache_allowed_hosts, true), "Cache list of allowed hosts",
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

  // `bind to` limits the listener to one local address, as on the other
  // servers; empty means every interface. An IPv6 address goes in brackets.
  boost::algorithm::trim(bind_to);
  const std::string bind = bind_to.empty() ? "0.0.0.0:" + port : bind_to.find(':') != std::string::npos ? "[" + bind_to + "]:" + port : bind_to + ":" + port;

  std::list<std::string> errors;
  socket_helpers::validate_certificate(certificate, errors);
  NSC_LOG_ERROR_LISTS(errors);
  const bool cert_missing = !boost::filesystem::is_regular_file(certificate);

  try {
    Mongoose::WebLoggerPtr logger(new net::web_server_logger(log_errors, log_info, log_debug, "NCPA: "));
    server_.reset(Mongoose::Server::make_server(logger));
    net::apply_tls_options(*server_, tls_version, allowed_ciphers);
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
      const std::string why = certificate.empty() ? std::string("no certificate is configured") : "no usable certificate at '" + certificate + "'";
      NSC_LOG_ERROR("NCPA: " + why + " and 'allow insecure = true' is set: serving UNENCRYPTED HTTP on port " + port + ". The token travels in clear.");
      // A server whose setSsl() failed refuses to start (it never falls back
      // to cleartext on its own), so the explicit opt-in gets a fresh one that
      // was never asked for TLS.
      if (!cert_missing) {
        server_.reset(Mongoose::Server::make_server(logger));
        net::apply_tls_options(*server_, tls_version, allowed_ciphers);
      }
    }
    // `allowed hosts` decides who may connect at all, so it is applied as a
    // connection is accepted - before the TLS handshake, before a worker is
    // spent on it, and whatever the request turns out to be.
    // The check itself never resolves anything (`cached` stays on): with
    // `cache allowed hosts = false` the names are re-resolved in the
    // background instead (host_refresher above).
    auto hosts = std::make_shared<socket_helpers::allowed_hosts_manager>();
    hosts->cached = true;
    hosts->set_source(allowed_hosts);
    {
      std::list<std::string> host_errors;
      hosts->refresh(host_errors);
      NSC_LOG_ERROR_LISTS(host_errors);
    }
    server_->setAcceptFilter([hosts](const std::string &remote) {
      std::list<std::string> host_errors;
      bool allowed = false;
      try {
        allowed = hosts->is_allowed(boost::asio::ip::make_address(remote), host_errors);
      } catch (const std::exception &e) {
        host_errors.push_back(std::string("unparsable peer address: ") + e.what());
      }
      if (!allowed) {
        NSC_LOG_ERROR("NCPA: rejected connection from " + remote + (host_errors.empty() ? std::string() : " (" + str::utils::joinEx(host_errors, ", ") + ")") +
                      ": not in 'allowed hosts'.");
      }
      return allowed;
    });
    // Checks run on a pool, so one slow check (an external script near its
    // timeout) does not hold up every other poll and TLS handshake.
    server_->setWorkerThreads(static_cast<std::size_t>(threads < 1 ? 1 : threads));
    // Its own thread name, so "Thread 'ncpa server': terminated ..." says
    // which listener died, and the guard line goes to the log unchanged.
    server_->setThreadReporting("ncpa server", NSC_THREAD_REPORTER);
    // No error sink is installed: it is process-global and the WEB server owns
    // it. The controller catches and logs its own failures.
    server_->registerController(new ncpa_controller(config, std::make_shared<ncpa_sources>(get_core(), get_id(), config.plugins.needs_module())));
    if (!server_->start(bind)) {
      NSC_LOG_ERROR("NCPA: the NCPA listener has NOT been started on port " + port + " (" + bind + "; see the error above). Fix the configuration and reload.");
      server_.reset();
      return true;
    }
    // Only once the listener is up: a failed start leaves nothing re-resolving
    // names for a listener that does not exist.
    if (!cache_allowed_hosts) {
      refresher_ = std::make_shared<host_refresher>();
      refresher_->hosts = hosts;
      const std::shared_ptr<host_refresher> refresher = refresher_;
      refresher_->thread = threads::start_guarded_thread("ncpa allowed hosts", [refresher] { refresher->run(); }, NSC_THREAD_REPORTER);
    }
    NSC_DEBUG_MSG("NCPA: listening on " + bind + (tls ? " (https" : " (http") + ", plugins = " + config.plugins.to_string() + ")");
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
