// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "IcingaAgent.h"

#include <config.h>

#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>
#include <str/xtos.hpp>
#include <threads/guarded_io_context.hpp>
#include <threads/guarded_thread.hpp>

#include "icinga_agent_connection.hpp"
#include "icinga_agent_pki.hpp"
#include "icinga_agent_protocol.hpp"

namespace sh = nscapi::settings_helper;
namespace asio = boost::asio;

namespace icinga_agent {

struct agent_config {
  connection_config connection;
  std::string endpoint_name;
  std::string certificate_directory;
  bool allow_arguments = true;
  std::string tls_version = "1.2+";
  std::string allowed_ciphers;
  std::string parent_fingerprint_setting;
  bool trust_on_first_use = true;
  int threads = 4;
  int reconnect_seconds = 10;
};

// The TLS settings of the agent as the options every socket module uses.
// Only the protocol part: the certificate, key, CA and verify mode depend on
// where the bootstrap stands and are filled in per connection.
inline socket_helpers::connection_info::ssl_opts tls_options(const agent_config &config) {
  socket_helpers::connection_info::ssl_opts opts;
  opts.enabled = true;
  opts.tls_version = config.tls_version;
  opts.allowed_ciphers = config.allowed_ciphers;
  opts.ssl_options = "default-workarounds";
  return opts;
}

// Owns the two io_contexts: one thread drives the connection, a small pool
// runs checks (a check blocks its thread until it returns).
class agent : public std::enable_shared_from_this<agent> {
 public:
  agent(agent_config config, nscapi::core_wrapper *core, const unsigned int plugin_id)
      : config_(std::move(config)),
        core_(core),
        plugin_id_(plugin_id),
        net_work_(asio::make_work_guard(net_io_)),
        check_work_(asio::make_work_guard(check_io_)),
        reconnect_timer_(net_io_),
        paths_(pki_paths::for_endpoint(config_.certificate_directory, config_.endpoint_name)) {}

  void start() {
    ensure_bootstrap_certificate(paths_, config_.endpoint_name);
    auto self = shared_from_this();
    threads_.push_back(threads::start_guarded_thread(
        "icinga agent connection", [self]() { threads::run_io_context_guarded("icinga agent connection", self->net_io_, NSC_THREAD_REPORTER); },
        NSC_THREAD_REPORTER));
    for (int i = 0; i < config_.threads; ++i) {
      threads_.push_back(threads::start_guarded_thread(
          "icinga agent checks", [self]() { threads::run_io_context_guarded("icinga agent checks", self->check_io_, NSC_THREAD_REPORTER); },
          NSC_THREAD_REPORTER));
    }
    asio::post(net_io_, [self]() { self->connect(); });
  }

  void stop() {
    auto self = shared_from_this();
    asio::post(net_io_, [self]() {
      self->stopping_ = true;
      self->reconnect_timer_.cancel();
      if (self->connection_) self->connection_->close("Agent is stopping");
      self->connection_.reset();
    });
    net_work_.reset();
    check_work_.reset();
    // Long-running checks are not interrupted; stop the loops and wait.
    net_io_.stop();
    check_io_.stop();
    for (auto &t : threads_) {
      if (t && t->joinable()) t->join();
    }
    threads_.clear();
  }

 private:
  void connect() {
    if (stopping_) return;
    try {
      // The context is built the way the other socket modules build theirs,
      // from the same options and the same `tls version` / `allowed ciphers`
      // vocabulary (socket_helpers::connection_info::ssl_opts).
      socket_helpers::connection_info::ssl_opts opts = tls_options(config_);
      opts.certificate = paths_.certificate;
      opts.certificate_key = paths_.key;
      if (boost::filesystem::exists(paths_.ca)) {
        opts.ca_path = paths_.ca;
        opts.verify_mode = "peer";
        config_.connection.verify_parent = true;
      } else if (!config_.connection.parent_fingerprint.empty()) {
        // No CA yet, but the parent's certificate is pinned, as with
        // `icinga2 node setup --trustedcert`: the connection checks the
        // fingerprint after the handshake and counts as verified.
        NSC_LOG_MESSAGE("Icinga agent: no CA at " + paths_.ca + " yet, verifying the parent by its pinned `parent fingerprint`");
        opts.verify_mode = "none";
        config_.connection.verify_parent = false;
      } else if (!config_.trust_on_first_use) {
        // Checked on every attempt, so a ca.crt put in place later is picked
        // up without a restart; said once, not every ten seconds.
        if (!refusal_logged_) {
          NSC_LOG_ERROR("Icinga agent: no CA at " + paths_.ca +
                        " and no `parent fingerprint`, and `trust on first use` is off: not connecting. Copy the "
                        "master's ca.crt there, or pin its certificate.");
          refusal_logged_ = true;
        }
        schedule_reconnect();
        return;
      } else {
        // Trust on first use, like `icinga2 node setup` without
        // `--trustedcert`: until the master has handed us its CA we cannot
        // verify it. The CA comes with the signed certificate; from then on
        // the parent is verified. Nothing but the certificate exchange happens
        // on such a connection: execute() refuses checks until the parent is
        // verified.
        NSC_LOG_ERROR("Icinga agent: no CA at " + paths_.ca + " yet, the parent's certificate is NOT verified on this connection; no checks run until it is");
        opts.verify_mode = "none";
        config_.connection.verify_parent = false;
      }
      ssl_ = std::make_shared<asio::ssl::context>(socket_helpers::tls_method_parser(opts.tls_version));
      ssl_->set_options(opts.get_ctx_opts());
      std::list<std::string> errors;
      opts.configure_ssl_context(*ssl_, errors);
      if (!errors.empty()) throw std::runtime_error(boost::algorithm::join(errors, "; "));
    } catch (const std::exception &e) {
      NSC_LOG_ERROR(std::string("Icinga agent: failed to set up TLS: ") + e.what());
      schedule_reconnect();
      return;
    }
    if (!is_signed(paths_.certificate)) {
      NSC_LOG_MESSAGE("Icinga agent: connecting with a self-signed certificate for '" + config_.endpoint_name + "' to request a signed one" +
                      (config_.connection.ticket.empty() ? " (no ticket: sign it on the master with `icinga2 ca sign`)" : ""));
    }
    auto self = shared_from_this();
    connection::callbacks cb;
    cb.log_info = [](const std::string &m) { NSC_LOG_MESSAGE("Icinga agent: " + m); };
    cb.log_error = [](const std::string &m) { NSC_LOG_ERROR("Icinga agent: " + m); };
    cb.log_debug = [](const std::string &m) { NSC_DEBUG_MSG("Icinga agent: " + m); };
    cb.execute = [self](std::shared_ptr<connection> conn, const boost::json::value &params) {
      asio::post(self->check_io_, [self, conn, params]() { self->execute(conn, params); });
    };
    cb.update_certificate = [self](const boost::json::value &params) { return self->update_certificate(params); };
    cb.closed = [self]() { asio::post(self->net_io_, [self]() { self->schedule_reconnect(); }); };
    connection_ = std::make_shared<connection>(net_io_, *ssl_, config_.connection, std::move(cb));
    connection_->start();
  }

  void schedule_reconnect() {
    if (stopping_) return;
    reconnect_timer_.expires_after(std::chrono::seconds(config_.reconnect_seconds));
    auto self = shared_from_this();
    reconnect_timer_.async_wait([self](const boost::system::error_code &ec) {
      if (!ec) self->connect();
    });
  }

  // True when a certificate was stored and the agent should reconnect with it.
  bool update_certificate(const boost::json::value &params) {
    try {
      const auto &obj = params.as_object();
      const auto cert = obj.find("cert");
      const auto ca = obj.find("ca");
      if (cert == obj.end() || !cert->value().is_string()) {
        NSC_LOG_ERROR("Icinga agent: pki::UpdateCertificate without a certificate");
        return false;
      }
      store_signed_certificate(paths_, std::string(cert->value().as_string()),
                               ca != obj.end() && ca->value().is_string() ? std::string(ca->value().as_string()) : std::string());
      NSC_LOG_MESSAGE("Icinga agent: stored the certificate signed by the Icinga CA in " + paths_.certificate);
      return true;
    } catch (const std::exception &e) {
      NSC_LOG_ERROR(std::string("Icinga agent: failed to store the signed certificate: ") + e.what());
      return false;
    }
  }

  static double now() { return std::chrono::duration<double>(std::chrono::system_clock::now().time_since_epoch()).count(); }

  void execute(const std::shared_ptr<connection> &conn, const boost::json::value &params) {
    const query_request req = parse_execute_command(params);
    check_result result;
    result.check_source = config_.endpoint_name;
    result.execution_start = now();
    result.command_line = req.query;
    for (const auto &a : req.arguments) result.command_line += " " + a;
    if (!conn->parent_verified()) {
      result.exit_status = 3;
      result.output = "UNKNOWN: the agent has not verified this parent against the Icinga CA yet and runs no checks for it";
    } else if (!req.error.empty()) {
      result.exit_status = 3;
      result.output = "UNKNOWN: " + req.error;
    } else if (!config_.allow_arguments && !req.arguments.empty()) {
      result.exit_status = 3;
      result.output = "UNKNOWN: arguments are not allowed on this agent (see /settings/Icinga/agent/allow arguments)";
    } else {
      try {
        nscapi::core_helper ch(core_, static_cast<int>(plugin_id_));
        std::string message, perf;
        // The verified parent's CN is the principal: the core permission
        // policy sees `IcingaAgent:<parent>`, as NRPE does with a client
        // certificate (docs/docs/concepts/permissions.md).
        result.exit_status = ch.simple_query_as(conn->parent_name(), req.query, req.arguments, message, perf, nscapi::protobuf::functions::no_truncation);
        result.output = message;
        result.perfdata = perf;
      } catch (const std::exception &e) {
        result.exit_status = 3;
        result.output = std::string("UNKNOWN: ") + e.what();
      }
    }
    result.execution_end = now();
    NSC_DEBUG_MSG("Icinga agent: " + req.host + (req.service.empty() ? "" : "!" + req.service) + " ran '" + result.command_line + "' -> " +
                  str::xtos(result.exit_status));
    conn->send(make_check_result(req.host, req.service, result));
  }

  agent_config config_;
  nscapi::core_wrapper *core_;
  unsigned int plugin_id_;
  asio::io_context net_io_;
  asio::io_context check_io_;
  asio::executor_work_guard<asio::io_context::executor_type> net_work_;
  asio::executor_work_guard<asio::io_context::executor_type> check_work_;
  asio::steady_timer reconnect_timer_;
  pki_paths paths_;
  std::shared_ptr<asio::ssl::context> ssl_;
  std::shared_ptr<connection> connection_;
  std::list<std::shared_ptr<boost::thread>> threads_;
  bool stopping_ = false;
  bool refusal_logged_ = false;
};

}  // namespace icinga_agent

IcingaAgent::IcingaAgent() = default;
IcingaAgent::~IcingaAgent() {
  try {
    stop();
  } catch (...) {
  }
}

void IcingaAgent::stop() {
  if (agent_) agent_->stop();
  agent_.reset();
}

bool IcingaAgent::loadModuleEx(std::string alias, const NSCAPI::moduleLoadMode mode) {
  stop();

  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("Icinga", std::move(alias), "agent");

  icinga_agent::agent_config config;
  std::string default_name = boost::to_lower_copy(asio::ip::host_name());

  // clang-format off
  settings.alias().add_path_to_settings()
    ("Icinga agent", "Native Icinga 2 agent: connect to an Icinga 2 parent over the cluster protocol and run the checks it schedules.")
  ;
  // clang-format on
  settings.alias()
      .add_key_to_settings()
      .add_string("parent host", sh::string_key(&config.connection.host), "PARENT HOST",
                  "Address of the Icinga 2 master or satellite to connect to. Empty (the default) leaves the agent disabled.")
      .add_string("parent port", sh::string_key(&config.connection.port, "5665"), "PARENT PORT", "The parent's cluster port.")
      .add_string("parent endpoint", sh::string_key(&config.connection.parent_endpoint), "PARENT ENDPOINT",
                  "The parent's endpoint name, the CN of its certificate. Required: a parent presenting any other certificate is refused, even one "
                  "the Icinga CA signed. It is also the principal checks run as, so the permission policy sees `IcingaAgent:<parent endpoint>`.")
      .add_string("endpoint name", sh::string_key(&config.endpoint_name, default_name), "ENDPOINT NAME",
                  "This agent's endpoint name, the CN of its certificate. Must match the Endpoint object on the master. Defaults to the "
                  "lower-cased host name.")
      .add_string("ticket", sh::string_key(&config.connection.ticket), "TICKET",
                  "A ticket from `icinga2 pki ticket --cn <endpoint name>` on the master. With it the master signs the agent's certificate "
                  "right away; without it the request waits for `icinga2 ca sign` on the master.")
      .add_string("parent fingerprint", sh::string_key(&config.parent_fingerprint_setting), "PARENT FINGERPRINT",
                  "SHA-256 fingerprint of the parent's certificate, as `openssl x509 -noout -fingerprint -sha256 -in <cert>` prints it "
                  "(colons and the `SHA256 Fingerprint=` prefix are optional). Pins the parent on the first connection, before the agent has "
                  "the Icinga CA - the equivalent of `icinga2 node setup --trustedcert`. Once the CA is stored the parent is verified against "
                  "it instead, so a renewed parent certificate keeps working. Empty (the default): no pin.")
      .add_bool("trust on first use", sh::bool_key(&config.trust_on_first_use, true), "TRUST ON FIRST USE",
                "Whether the agent may make its first connection without being able to verify the parent: no `ca.crt` in the certificate "
                "directory yet and no `parent fingerprint`. Only the certificate exchange happens on such a connection, never a check. "
                "True (the default) is how `icinga2 node setup` behaves without `--trustedcert`; set it to false to require the CA or the "
                "pin up front.")
      .add_string("certificate directory", sh::path_key(&config.certificate_directory, "${certificate-path}/icinga"), "CERTIFICATE DIRECTORY",
                  "Where the agent keeps <endpoint name>.crt, <endpoint name>.key and ca.crt. Generated on first start.")
      .add_bool("allow arguments", sh::bool_key(&config.allow_arguments, true), "ALLOW ARGUMENTS",
                "Whether the parent may pass arguments to the checks it runs. True (the default) because Icinga CheckCommands carry their "
                "thresholds as arguments; false runs only argument-less queries and aliases defined on the agent. Which queries the parent may "
                "run at all is the permission policy's job (/settings/permissions).")
      .add_string("tls version", sh::string_key(&config.tls_version, "1.2+"), "TLS VERSION",
                  "Which TLS versions the agent negotiates with its parent, in the same vocabulary as the other servers and clients: an exact "
                  "version (1.0, 1.1, 1.2, 1.3), a trailing + for that version or later, or `any`. The default, 1.2+, is what Icinga 2 accepts "
                  "out of the box.")
      .add_string("allowed ciphers", sh::string_key(&config.allowed_ciphers), "ALLOWED CIPHERS",
                  "OpenSSL cipher list for TLS 1.2 and earlier. Empty (the default) leaves the library's own selection in place. TLS 1.3 "
                  "suites are not affected by it.")
      .add_int("threads", sh::int_key(&config.threads, 4), "WORKER THREADS", "How many checks run at the same time.");

  settings.register_all();
  settings.notify();

  if (mode != NSCAPI::normalStart && mode != NSCAPI::reloadStart) return true;
  if (config.connection.host.empty()) {
    NSC_LOG_MESSAGE("Icinga agent: no parent host configured under /settings/Icinga/agent, not connecting");
    return true;
  }
  if (config.connection.parent_endpoint.empty()) {
    NSC_LOG_ERROR(
        "Icinga agent: `parent endpoint` is not set under /settings/Icinga/agent: refusing to connect, since any certificate "
        "the Icinga CA signed could then act as the parent");
    return true;
  }
  if (!socket_helpers::is_valid_peer_principal(config.connection.parent_endpoint)) {
    NSC_LOG_ERROR("Icinga agent: `parent endpoint` '" + socket_helpers::escape_for_log(config.connection.parent_endpoint) +
                  "' cannot be a permission principal (no ':', '=' or control characters): not connecting");
    return true;
  }
  if (!config.parent_fingerprint_setting.empty() &&
      !icinga_agent::normalize_fingerprint(config.parent_fingerprint_setting, config.connection.parent_fingerprint)) {
    NSC_LOG_ERROR("Icinga agent: `parent fingerprint` is not a SHA-256 certificate fingerprint (64 hex digits): not connecting");
    return true;
  }
  {
    // Refuse a bad `tls version` or `allowed ciphers` here, once, rather than
    // on every reconnect attempt.
    try {
      const socket_helpers::connection_info::ssl_opts opts = icinga_agent::tls_options(config);
      asio::ssl::context probe(socket_helpers::tls_method_parser(opts.tls_version));
      std::list<std::string> errors;
      opts.configure_ssl_context(probe, errors);
      if (!errors.empty()) throw std::runtime_error(boost::algorithm::join(errors, "; "));
    } catch (const std::exception &e) {
      NSC_LOG_ERROR(std::string("Icinga agent: invalid TLS settings under /settings/Icinga/agent: ") + e.what() + ": not connecting");
      return true;
    }
  }
  boost::to_lower(config.endpoint_name);
  config.connection.agent_version = std::string("NSClient++/") + STRPRODUCTVER;
  if (config.threads < 1) config.threads = 1;
  try {
    agent_ = std::make_shared<icinga_agent::agent>(config, get_core(), get_id());
    agent_->start();
  } catch (const std::exception &e) {
    NSC_LOG_ERROR(std::string("Icinga agent: failed to start: ") + e.what());
    agent_.reset();
  }
  return true;
}

void IcingaAgent::prepareShutdown() {
  try {
    stop();
  } catch (...) {
    NSC_LOG_ERROR_EX("prepare_shutdown");
  }
}

bool IcingaAgent::unloadModule() {
  try {
    stop();
  } catch (...) {
    NSC_LOG_ERROR_EX("unload");
    return false;
  }
  return true;
}
