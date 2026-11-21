// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "IcingaAgent.h"

#include <config.h>

#include <atomic>
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/filesystem.hpp>
#include <boost/thread/thread.hpp>
#include <chrono>
#include <fstream>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_program_options.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/functions_response.hpp>
#include <nscapi/protobuf/settings_functions.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <sstream>
#include <threads/guarded_io_context.hpp>
#include <threads/guarded_thread.hpp>

#include "icinga_agent_connection.hpp"
#include "icinga_agent_import.hpp"
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
  translation_config translation;
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
    if (ensure_bootstrap_certificate(paths_, config_.endpoint_name)) {
      NSC_LOG_ERROR("Icinga agent: the certificate and key in " + config_.certificate_directory + " for '" + config_.endpoint_name +
                    "' could not be used together and were replaced with a new self-signed pair; the parent has to sign it again");
    }
    auto self = shared_from_this();
    threads_.push_back(threads::start_guarded_thread(
        "icinga agent connection", [self]() { threads::run_io_context_guarded("icinga agent connection", self->net_io_, NSC_THREAD_REPORTER); },
        NSC_THREAD_REPORTER));
    for (int i = 0; i < config_.threads; ++i) {
      threads_.push_back(threads::start_guarded_thread(
          "icinga agent checks", [self]() { threads::run_io_context_guarded("icinga agent checks", self->check_io_, NSC_THREAD_REPORTER); },
          NSC_THREAD_REPORTER));
    }
    const std::weak_ptr<agent> weak = self;
    asio::post(net_io_, [weak]() {
      if (const auto me = weak.lock()) me->connect();
    });
  }

  // Only the threads own the agent (through `self` above). Every handler and
  // connection callback holds a weak_ptr instead: a handler still queued when
  // the loops stop would otherwise keep the agent alive from inside the
  // io_context the agent itself owns, and so would the connection through its
  // callbacks.
  void stop() {
    stopping_ = true;
    net_work_.reset();
    check_work_.reset();
    // Long-running checks are not interrupted; stop the loops and wait.
    net_io_.stop();
    check_io_.stop();
    for (auto &t : threads_) {
      if (t && t->joinable()) t->join();
    }
    threads_.clear();
    // The threads are gone, so nothing else touches the connection: close it
    // here rather than in a handler posted to a loop that is being stopped,
    // which may never run and would leave the socket open to the parent.
    reconnect_timer_.cancel();
    if (connection_) connection_->close("Agent is stopping");
    connection_.reset();
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
        if (!config_.connection.ticket.empty()) {
          NSC_LOG_ERROR(
              "Icinga agent: the `ticket` is sent over this unverified connection, to whoever answers at " + config_.connection.host + ":" +
              config_.connection.port +
              ". Anyone holding it can have the master sign a certificate for '" + config_.endpoint_name +
              "'. Install the master's ca.crt or set `parent fingerprint` to verify the parent first.");
        }
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
    const std::weak_ptr<agent> weak = shared_from_this();
    connection::callbacks cb;
    cb.log_info = [](const std::string &m) { NSC_LOG_MESSAGE("Icinga agent: " + m); };
    cb.log_error = [](const std::string &m) { NSC_LOG_ERROR("Icinga agent: " + m); };
    cb.log_debug = [](const std::string &m) { NSC_DEBUG_MSG("Icinga agent: " + m); };
    cb.execute = [weak](std::shared_ptr<connection> conn, const boost::json::value &params) {
      const auto me = weak.lock();
      if (!me) return;
      asio::post(me->check_io_, [weak, conn, params]() {
        if (const auto agent = weak.lock()) agent->execute(conn, params);
      });
    };
    cb.update_certificate = [weak](const boost::json::value &params) {
      const auto me = weak.lock();
      return me && me->update_certificate(params);
    };
    cb.closed = [weak]() {
      const auto me = weak.lock();
      if (!me) return;
      asio::post(me->net_io_, [weak]() {
        if (const auto agent = weak.lock()) agent->schedule_reconnect();
      });
    };
    connection_ = std::make_shared<connection>(net_io_, *ssl_, config_.connection, std::move(cb));
    connection_->start();
  }

  void schedule_reconnect() {
    if (stopping_) return;
    reconnect_timer_.expires_after(std::chrono::seconds(config_.reconnect_seconds));
    const std::weak_ptr<agent> weak = shared_from_this();
    reconnect_timer_.async_wait([weak](const boost::system::error_code &ec) {
      if (ec) return;
      if (const auto me = weak.lock()) me->connect();
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
      store_signed_certificate(paths_, config_.endpoint_name, std::string(cert->value().as_string()),
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
    const query_request req = parse_execute_command(params, config_.translation);
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
    } else if (!config_.allow_arguments && !req.arguments.empty() && !req.fixed_arguments) {
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
  // Set by stop() on the caller's thread, read on the connection thread.
  std::atomic<bool> stopping_{false};
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
    ("commands", sh::fun_values_path([&config](auto key, auto value) {
        try {
          icinga_agent::command_alias alias;
          str::utils::parse_alias_command(value, alias.query, alias.arguments);
          if (alias.query.empty()) throw std::invalid_argument("no query");
          config.translation.aliases[key] = alias;
        } catch (const std::exception &e) {
          NSC_LOG_ERROR("Icinga agent: refusing the command alias '" + key + "': " + utf8::utf8_from_native(e.what()));
        }
      }),
      "Icinga command aliases",
      "Icinga check commands this agent answers as an NSClient++ query: `<Icinga command> = <query> <argument>...`. "
      "An argument may use the macros the master sends with the check, as the command's own arguments do on the master "
      "(`disk-windows = check_drivesize drive=$disk_win_path$`): an argument whose macro has no value is left out, "
      "and one that is a single list macro becomes one argument per element. "
      "An alias here takes precedence over the built-in translation of the same command (see `translate commands`). "
      "Commands that import `ifw-api` name their own query and are not looked up here.",
      "ICINGA COMMAND", "The NSClient++ query and arguments to run for this Icinga check command")
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
      .add_password("ticket", sh::string_key(&config.connection.ticket), "TICKET",
                    "A ticket from `icinga2 pki ticket --cn <endpoint name>` on the master. With it the master signs the agent's certificate "
                    "right away; without it the request waits for `icinga2 ca sign` on the master. Treat it as a password: it does not expire, "
                    "and whoever holds it can have the master sign a certificate for this endpoint name. The agent sends it to the parent on "
                    "every connect, so on a first connection that is not verified (no `ca.crt`, no `parent fingerprint`) it goes to whoever "
                    "answers at `parent host`.")
      .add_string("parent fingerprint", sh::string_key(&config.parent_fingerprint_setting), "PARENT FINGERPRINT",
                  "SHA-256 fingerprint of the parent's certificate, as `openssl x509 -noout -fingerprint -sha256 -in <cert>` prints it "
                  "(colons and the `SHA256 Fingerprint=` prefix are optional). Pins the parent on the first connection, before the agent has "
                  "the Icinga CA - the equivalent of `icinga2 node setup --trustedcert`. Once the CA is stored the parent is verified against "
                  "it instead, so a renewed parent certificate keeps working. Empty (the default): no pin.")
      .add_bool("trust on first use", sh::bool_key(&config.trust_on_first_use, true), "TRUST ON FIRST USE",
                "Whether the agent may make its first connection without being able to verify the parent: no `ca.crt` in the certificate "
                "directory yet and no `parent fingerprint`. Only the certificate exchange happens on such a connection, never a check - but "
                "the exchange includes the `ticket`, and the CA the agent trusts from then on is the one that peer sends. "
                "True (the default) is how `icinga2 node setup` behaves without `--trustedcert`; set it to false to require the CA or the "
                "pin up front.")
      .add_string("certificate directory", sh::path_key(&config.certificate_directory, "${certificate-path}/icinga"), "CERTIFICATE DIRECTORY",
                  "Where the agent keeps <endpoint name>.crt, <endpoint name>.key and ca.crt. Generated on first start.")
      .add_bool("allow arguments", sh::bool_key(&config.allow_arguments, true), "ALLOW ARGUMENTS",
                "Whether the parent may pass arguments to the checks it runs. True (the default) because Icinga CheckCommands carry their "
                "thresholds as arguments; false runs only argument-less queries and aliases defined on the agent. Which queries the parent may "
                "run at all is the permission policy's job (/settings/permissions).")
      .add_bool("translate commands", sh::bool_key(&config.translation.builtin, true), "TRANSLATE ICINGA COMMANDS",
                "Whether the agent answers the Windows check commands of the Icinga Template Library under their Icinga names: "
                "`disk-windows`, `load-windows`, `memory-windows`, `swap-windows`, `service-windows`, `procs-windows`, `uptime-windows`, "
                "`users-windows`, `network-windows`, `perfmon-windows`, `update-windows`, `ping-windows` and the `nscp-local` commands. "
                "The master sends such a command with the variables the service set (`disk_win_warn`, `disk_win_path`, ...); the agent "
                "turns them into the matching query and its `-w` / `-c`, so a host that ran the Icinga 2 agent keeps its services "
                "unchanged. True (the default); false runs only `ifw-api` commands, the aliases under `commands` and queries called "
                "by their own name.")
      .add_string("tls version", sh::string_key(&config.tls_version, "1.2+"), "TLS VERSION",
                  "Which TLS versions the agent negotiates with its parent, in the same vocabulary as the other servers and clients: an exact "
                  "version (1.0, 1.1, 1.2, 1.3), a trailing + for that version or later, or `any`. The default, 1.2+, is what Icinga 2 accepts "
                  "out of the box.")
      .add_string("allowed ciphers", sh::string_key(&config.allowed_ciphers), "ALLOWED CIPHERS",
                  "OpenSSL cipher list for TLS 1.2 and earlier. Empty (the default) leaves the library's own selection in place. TLS 1.3 "
                  "suites are not affected by it.")
      .add_int("threads", sh::int_key(&config.threads, 4), "WORKER THREADS", "How many checks run at the same time.")
      .add_int("certificate request interval", sh::int_key(&config.connection.certificate_request_interval, 24 * 60 * 60), "CERTIFICATE REQUEST INTERVAL",
               "Seconds between requests for a renewed certificate on an open connection; one is also sent on every connect. The parent only "
               "issues a new certificate within 30 days of the current one expiring (or when its CA is newer than the agent's), so the default, "
               "a day like the Icinga agent's, renews in time on a connection that stays up for months. 0 asks only on connect.");

  settings.register_all();
  settings.notify();
  certificate_directory_ = config.certificate_directory;

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

namespace {
namespace po = boost::program_options;
namespace fs = boost::filesystem;

const char *const agent_settings_path = "/settings/Icinga/agent";

// Where the Icinga 2 agent keeps its data: one tree on Windows, the usual
// places under / elsewhere.
std::string default_icinga_root() {
#ifdef WIN32
  const char *program_data = std::getenv("ProgramData");
  return (fs::path(program_data ? program_data : "C:\\ProgramData") / "icinga2").string();
#else
  return "/";
#endif
}

bool read_file(const fs::path &path, std::string &content) {
  std::ifstream in(path.string().c_str(), std::ios::binary);
  if (!in) return false;
  std::ostringstream buffer;
  buffer << in.rdbuf();
  content = buffer.str();
  return true;
}

std::string required_file(const fs::path &path, const std::string &what) {
  std::string content;
  if (!read_file(path, content)) throw std::invalid_argument("Cannot read " + what + " at " + path.string());
  return content;
}

// The agent's name when nothing says it: the one key in the directory.
std::string name_of_only_key(const fs::path &certificates) {
  std::string name;
  boost::system::error_code ec;
  for (fs::directory_iterator it(certificates, ec), end; !ec && it != end; it.increment(ec)) {
    if (it->path().extension() != ".key") continue;
    if (!name.empty()) return "";
    name = it->path().stem().string();
  }
  return name;
}
}  // namespace

bool IcingaAgent::commandLineExec(const int, const PB::Commands::ExecuteRequestMessage::Request &request,
                                  PB::Commands::ExecuteResponseMessage::Response *response, const PB::Commands::ExecuteRequestMessage &) {
  if (request.arguments_size() > 0 && request.arguments(0) == "import") return import_agent(request, response);
  nscapi::protobuf::functions::set_response_bad(*response, "Usage: nscp icinga-agent import --help");
  return true;
}

bool IcingaAgent::import_agent(const PB::Commands::ExecuteRequestMessage::Request &request, PB::Commands::ExecuteResponseMessage::Response *response) const {
  namespace pf = nscapi::protobuf::functions;

  std::string from = default_icinga_root(), certificates, config, endpoint_name, parent_endpoint, parent_host, parent_port;
  bool dry_run = false, force = false;
  po::options_description desc("nscp icinga-agent import: take over the identity of the Icinga 2 agent on this host");
  // clang-format off
  desc.add_options()
    ("help", "Show help.")
    ("from", po::value<std::string>(&from)->default_value(from),
      "The Icinga 2 agent's data directory, holding var/lib/icinga2/certs and etc/icinga2.")
    ("certificates", po::value<std::string>(&certificates),
      "The directory with <name>.crt, <name>.key and ca.crt, when it is not <from>/var/lib/icinga2/certs.")
    ("config", po::value<std::string>(&config),
      "The directory with constants.conf and zones.conf, when it is not <from>/etc/icinga2.")
    ("endpoint-name", po::value<std::string>(&endpoint_name),
      "This agent's endpoint name. Default: NodeName in constants.conf, else the name of the only key among the certificates.")
    ("parent-endpoint", po::value<std::string>(&parent_endpoint),
      "The parent's endpoint name. Default: the one other Endpoint in zones.conf.")
    ("parent-host", po::value<std::string>(&parent_host),
      "The parent's address. Default: `host` of that Endpoint in zones.conf.")
    ("parent-port", po::value<std::string>(&parent_port),
      "The parent's port. Default: `port` of that Endpoint, else 5665.")
    ("force", po::value<bool>(&force)->implicit_value(true)->default_value(false),
      "Replace a different key that is already in the certificate directory.")
    ("dry-run", po::value<bool>(&dry_run)->implicit_value(true)->default_value(false),
      "Show what would be imported and written, and change nothing.")
    ;
  // clang-format on

  try {
    std::vector<std::string> args;
    for (int i = 1; i < request.arguments_size(); ++i) args.push_back(request.arguments(i));
    po::variables_map vm;
    po::store(nscapi::program_options::basic_command_line_parser(args).options(desc).run(), vm);
    po::notify(vm);

    if (vm.count("help")) {
      pf::set_response_good(
          *response,
          nscapi::program_options::help(
              desc,
              "Usage: nscp icinga-agent import [--from <icinga2 data directory>] [--parent-host <address>] [--dry-run]\n"
              "Copies the Icinga 2 agent's key, certificate and CA into this agent's certificate directory and points\n"
              "/settings/Icinga/agent at the same parent, so NSClient++ connects as the endpoint the master already knows.\n"
              "Stop the Icinga 2 agent before NSClient++ is restarted: both cannot be connected as the same endpoint.\n"));
      return true;
    }

    const fs::path certificate_source = certificates.empty() ? fs::path(from) / "var" / "lib" / "icinga2" / "certs" : fs::path(certificates);
    const fs::path config_source = config.empty() ? fs::path(from) / "etc" / "icinga2" : fs::path(config);

    // Who the agent is.
    std::string constants, zones;
    read_file(config_source / "constants.conf", constants);
    read_file(config_source / "zones.conf", zones);
    if (endpoint_name.empty()) endpoint_name = icinga_agent::parse_node_name(constants);
    if (endpoint_name.empty()) endpoint_name = name_of_only_key(certificate_source);
    if (endpoint_name.empty()) {
      throw std::invalid_argument("Cannot tell this agent's name: no NodeName in " + (config_source / "constants.conf").string() + " and not exactly one .key in " +
                                  certificate_source.string() + ". Give --endpoint-name (and --from, if the Icinga 2 agent is installed elsewhere).");
    }

    // Who it reports to.
    const std::vector<icinga_agent::icinga_endpoint> parents = icinga_agent::parent_endpoints(icinga_agent::parse_endpoints(zones), endpoint_name);
    icinga_agent::icinga_endpoint parent;
    if (!parent_endpoint.empty()) {
      parent.name = parent_endpoint;
      for (const icinga_agent::icinga_endpoint &candidate : parents) {
        if (candidate.name == parent_endpoint) parent = candidate;
      }
    } else if (parents.size() == 1) {
      parent = parents.front();
    } else if (parents.empty()) {
      throw std::invalid_argument("No parent Endpoint in " + (config_source / "zones.conf").string() + ". Give --parent-endpoint and --parent-host.");
    } else {
      std::string names;
      for (const icinga_agent::icinga_endpoint &candidate : parents) names += (names.empty() ? "" : ", ") + candidate.name;
      throw std::invalid_argument("The Icinga agent has " + str::xtos(parents.size()) + " parents (" + names +
                                  "). NSClient++ connects to one: choose it with --parent-endpoint.");
    }
    if (!parent_host.empty()) parent.host = parent_host;
    if (!parent_port.empty()) parent.port = parent_port;
    if (parent.host.empty()) {
      throw std::invalid_argument("The Endpoint '" + parent.name +
                                  "' has no host in zones.conf: the parent connected to the Icinga agent. NSClient++ connects out, so give "
                                  "--parent-host, and make sure this host can reach it on port 5665.");
    }
    if (!socket_helpers::is_valid_peer_principal(parent.name)) {
      throw std::invalid_argument("'" + socket_helpers::escape_for_log(parent.name) + "' cannot be used as `parent endpoint`");
    }

    const std::string cert_pem = required_file(certificate_source / (endpoint_name + ".crt"), "the certificate");
    const std::string key_pem = required_file(certificate_source / (endpoint_name + ".key"), "the key");
    const std::string ca_pem = required_file(certificate_source / "ca.crt", "the Icinga CA");

    if (certificate_directory_.empty()) throw std::invalid_argument("No `certificate directory` is configured under /settings/Icinga/agent");
    const icinga_agent::pki_paths paths = icinga_agent::pki_paths::for_endpoint(certificate_directory_, endpoint_name);
    std::string existing_key;
    if (read_file(paths.key, existing_key) && existing_key != key_pem && !force) {
      throw std::invalid_argument("There is already a different key for '" + endpoint_name + "' at " + paths.key +
                                  ". Importing replaces this agent's own identity: pass --force if that is what you want.");
    }

    std::stringstream result;
    result << (dry_run ? "Would import" : "Imported") << " the Icinga 2 agent '" << endpoint_name << "' from " << certificate_source.string() << std::endl;
    result << "  key:         " << paths.key << std::endl;
    result << "  certificate: " << paths.certificate << std::endl;
    result << "  CA:          " << paths.ca << std::endl;
    result << "Parent: " << parent.name << " at " << parent.host << ":" << (parent.port.empty() ? "5665" : parent.port) << std::endl;
    if (dry_run) {
      result << "Nothing was changed (--dry-run)." << std::endl;
      pf::set_response_good(*response, result.str());
      return true;
    }

    icinga_agent::import_identity(paths, endpoint_name, cert_pem, key_pem, ca_pem);

    pf::settings_query s(get_id());
    s.set(MAIN_MODULES_SECTION, "IcingaAgent", "enabled");
    s.set(agent_settings_path, "endpoint name", endpoint_name);
    s.set(agent_settings_path, "parent endpoint", parent.name);
    s.set(agent_settings_path, "parent host", parent.host);
    if (!parent.port.empty()) s.set(agent_settings_path, "parent port", parent.port);
    // The CA is in place, so there is never a reason to connect unverified.
    s.set(agent_settings_path, "trust on first use", "false");
    s.save();
    get_core()->settings_query(s.request(), s.response());
    if (!s.validate_response()) {
      pf::set_response_bad(*response, "The certificates were imported, but the settings could not be written: " + s.get_response_error());
      return true;
    }
    result << "Settings written to " << agent_settings_path << "; the IcingaAgent module is enabled." << std::endl;
    result << "Next: stop the Icinga 2 agent, then restart NSClient++. Both cannot be connected as '" << endpoint_name << "' at once." << std::endl;
    result << "The Icinga 2 agent's copy of the key is still at " << (certificate_source / (endpoint_name + ".key")).string()
           << ": delete it once that agent is uninstalled." << std::endl;
    pf::set_response_good(*response, result.str());
    return true;
  } catch (const std::invalid_argument &e) {
    pf::set_response_bad(*response, e.what());
    return true;
  } catch (const std::runtime_error &e) {
    pf::set_response_bad(*response, std::string("Not imported: ") + utf8::utf8_from_native(e.what()));
    return true;
  } catch (...) {
    nscapi::program_options::report_command_exception(desc, request.command(), *response, "Failed to import the Icinga 2 agent");
    return true;
  }
}
