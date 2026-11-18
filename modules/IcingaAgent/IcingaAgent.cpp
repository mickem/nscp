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
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <set>
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
  std::string allowed_commands;  // "any" or a comma separated list
  int threads = 4;
  int reconnect_seconds = 10;
};

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
        paths_(pki_paths::for_endpoint(config_.certificate_directory, config_.endpoint_name)) {
    if (config_.allowed_commands != "any") {
      std::vector<std::string> list;
      boost::split(list, config_.allowed_commands, boost::is_any_of(","));
      for (auto &c : list) {
        boost::trim(c);
        if (!c.empty()) allowed_.insert(boost::to_lower_copy(c));
      }
    }
  }

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
      ssl_ = std::make_shared<asio::ssl::context>(asio::ssl::context::tls_client);
      ssl_->set_options(asio::ssl::context::default_workarounds | asio::ssl::context::no_sslv2 | asio::ssl::context::no_sslv3 | asio::ssl::context::no_tlsv1 |
                        asio::ssl::context::no_tlsv1_1);
      ssl_->use_certificate_chain_file(paths_.certificate);
      ssl_->use_private_key_file(paths_.key, asio::ssl::context::pem);
      if (boost::filesystem::exists(paths_.ca)) {
        ssl_->load_verify_file(paths_.ca);
        ssl_->set_verify_mode(asio::ssl::verify_peer);
      } else {
        // Trust on first use, like `icinga2 node setup --trustedcert`: until
        // the master has handed us its CA we cannot verify it. The CA comes
        // with the signed certificate; from then on the parent is verified.
        NSC_LOG_ERROR("Icinga agent: no CA at " + paths_.ca + " yet, the parent's certificate is NOT verified on this connection");
        ssl_->set_verify_mode(asio::ssl::verify_none);
      }
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
    cb.update_certificate = [self](const boost::json::value &params) { self->update_certificate(params); };
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

  void update_certificate(const boost::json::value &params) {
    try {
      const auto &obj = params.as_object();
      const auto cert = obj.find("cert");
      const auto ca = obj.find("ca");
      if (cert == obj.end() || !cert->value().is_string()) {
        NSC_LOG_ERROR("Icinga agent: pki::UpdateCertificate without a certificate");
        return;
      }
      store_signed_certificate(paths_, std::string(cert->value().as_string()),
                               ca != obj.end() && ca->value().is_string() ? std::string(ca->value().as_string()) : std::string());
      NSC_LOG_MESSAGE("Icinga agent: stored the certificate signed by the Icinga CA in " + paths_.certificate);
    } catch (const std::exception &e) {
      NSC_LOG_ERROR(std::string("Icinga agent: failed to store the signed certificate: ") + e.what());
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
    if (!req.error.empty()) {
      result.exit_status = 3;
      result.output = "UNKNOWN: " + req.error;
    } else if (!allowed_.empty() && allowed_.count(boost::to_lower_copy(req.query)) == 0) {
      result.exit_status = 3;
      result.output = "UNKNOWN: command '" + req.query + "' is not allowed on this agent (see /settings/Icinga/agent/allowed commands)";
    } else {
      try {
        nscapi::core_helper ch(core_, static_cast<int>(plugin_id_));
        std::string message, perf;
        result.exit_status = ch.simple_query(req.query, req.arguments, message, perf, nscapi::protobuf::functions::no_truncation);
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
  std::set<std::string> allowed_;
  std::shared_ptr<asio::ssl::context> ssl_;
  std::shared_ptr<connection> connection_;
  std::list<std::shared_ptr<boost::thread>> threads_;
  bool stopping_ = false;
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
                  "The parent's endpoint name (its certificate CN). When set, a parent presenting any other certificate is refused.")
      .add_string("endpoint name", sh::string_key(&config.endpoint_name, default_name), "ENDPOINT NAME",
                  "This agent's endpoint name, the CN of its certificate. Must match the Endpoint object on the master. Defaults to the "
                  "lower-cased host name.")
      .add_string("ticket", sh::string_key(&config.connection.ticket), "TICKET",
                  "A ticket from `icinga2 pki ticket --cn <endpoint name>` on the master. With it the master signs the agent's certificate "
                  "right away; without it the request waits for `icinga2 ca sign` on the master.")
      .add_string("certificate directory", sh::path_key(&config.certificate_directory, "${certificate-path}/icinga"), "CERTIFICATE DIRECTORY",
                  "Where the agent keeps <endpoint name>.crt, <endpoint name>.key and ca.crt. Generated on first start.")
      .add_string("allowed commands", sh::string_key(&config.allowed_commands, "any"), "ALLOWED COMMANDS",
                  "Which queries the parent may run: `any` or a comma separated list of query names.")
      .add_int("threads", sh::int_key(&config.threads, 4), "WORKER THREADS", "How many checks run at the same time.");

  settings.register_all();
  settings.notify();

  if (mode != NSCAPI::normalStart && mode != NSCAPI::reloadStart) return true;
  if (config.connection.host.empty()) {
    NSC_LOG_MESSAGE("Icinga agent: no parent host configured under /settings/Icinga/agent, not connecting");
    return true;
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
