// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "NCPAServer.h"

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/operations.hpp>
#include <list>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/protobuf/functions_perfdata.hpp>
#include <nscapi/protobuf/registry.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <utility>

namespace sh = nscapi::settings_helper;

namespace {

// Routes the HTTP layer's own logging into the agent log, the way WEBServer
// does. Errors are on by default and the chattier levels are not: an NCPA
// listener is polled every minute by every host that watches this machine.
class ncpa_logger : public Mongoose::WebLogger {
 public:
  ncpa_logger(const bool log_errors, const bool log_info, const bool log_debug) : log_errors_(log_errors), log_info_(log_info), log_debug_(log_debug) {}
  void log_error(const std::string &message) override {
    if (log_errors_) NSC_LOG_ERROR(message);
  }
  void log_info(const std::string &message) override {
    if (log_info_) NSC_LOG_MESSAGE(message);
  }
  void log_debug(const std::string &message) override {
    if (log_debug_) NSC_DEBUG_MSG(message);
  }

 private:
  bool log_errors_;
  bool log_info_;
  bool log_debug_;
};

// The `plugins/` node's way into the agent: any registered query, alias or
// external script, with its output handed back unchanged.
class core_dispatcher : public ncpa::query_dispatcher {
 public:
  core_dispatcher(const nscapi::core_wrapper *core, const unsigned int plugin_id, ncpa::session_ptr session)
      : core_(core), plugin_id_(plugin_id), session_(std::move(session)) {}

  ncpa::check_result run_query(const std::string &name, const std::vector<std::string> &args) override {
    ncpa::check_result result;
    // is_exposed, not session_->exposes: `plugins = scripts` is resolved
    // against the core's registry, and asking the session directly would
    // refuse every script. Belt and braces either way - the tree already
    // refuses an unexposed name before it gets here.
    if (!is_exposed(name)) {
      result.returncode = 3;
      result.stdout_text = "UNKNOWN: The plugin (" + name + ") requested does not exist.";
      return result;
    }
    // Every path segment after the plugin name is passed on verbatim as one
    // `key=value` token - the same REST-style token the web API accepts, which
    // is exactly what check_ncpa.py's `-a` produces once its shell-style
    // splitting is done.
    const std::list<std::string> arguments(args.begin(), args.end());
    std::string message;
    std::string perf;
    const NSCAPI::nagiosReturn code =
        nscapi::core_helper(core_, plugin_id_).simple_query(name, arguments, message, perf, nscapi::protobuf::functions::no_truncation);
    result.returncode = code;
    // The plugin's own output, unchanged: this is the one node whose text is
    // not reformatted, which is what lets every NSClient++ check be reached
    // from Nagios without any mapping work.
    result.stdout_text = perf.empty() ? message : message + "|" + perf;
    return result;
  }

  std::vector<std::string> list_queries() override {
    // An explicit allow-list is the answer on its own: asking the core for an
    // inventory would only be filtered back down to it.
    if (!session_->exposes_everything() && !session_->scripts_only()) return session_->plugin_allow_list();

    std::vector<std::string> out;
    for (const inventory_entry &entry : inventory()) {
      if (session_->scripts_only() ? is_external_script(entry) : session_->exposes(entry.name)) out.push_back(entry.name);
    }
    return out;
  }

  bool is_exposed(const std::string &name) override {
    if (!session_->scripts_only()) return session_->exposes(name);
    // `plugins = scripts` exposes what an external-scripts module registered,
    // which is a question only the core can answer - so ask it rather than
    // guessing from the name (a script may perfectly well be called
    // `check_backup`, and a built-in may not start with `check_`).
    //
    // The whole inventory, not a lookup by name: a name-scoped inventory
    // answers without the plugin field this needs. It is a registry walk, not
    // a check run (fetch_all stays off), and only `plugins = scripts` pays
    // for it.
    for (const inventory_entry &entry : inventory()) {
      if (entry.name == name) return is_external_script(entry);
    }
    return false;
  }
  bool allow_arguments() override { return session_->allow_arguments(); }

  std::vector<ncpa::service_entry> list_services() override {
    std::vector<ncpa::service_entry> out;
    // The agent's own service check, asked for a machine-readable listing
    // rather than a verdict: no thresholds, one record per line, tab
    // separated. `list-separator` takes the escape, and the filter's own
    // matching is switched off because the NCPA filters (`match=search`,
    // `status=`) have NCPA's semantics and are applied on the tree side.
    for (const std::vector<std::string> &row : tabulate("check_service", {"name", "state"})) {
      ncpa::service_entry entry;
      entry.name = row[0];
      // NSClient++ reports the platform's own state word (`running`,
      // `started`, `active`, `stopped`, `dead`, ...); NCPA knows only two.
      entry.status = is_running(row[1]) ? "running" : "stopped";
      out.push_back(entry);
    }
    return out;
  }

  std::vector<ncpa::process_entry> list_processes() override {
    std::vector<ncpa::process_entry> out;
    // `command_line` last: it is the one field that can itself contain a tab,
    // and the splitter keeps everything after the last expected separator.
    // `resolve-owner=true`: without it check_process leaves `username` empty,
    // and NCPA's `username` filter would then match nothing at all.
    for (const std::vector<std::string> &row : tabulate("check_process", {"exe", "filename", "username", "pid", "command_line"}, {"resolve-owner=true"})) {
      ncpa::process_entry entry;
      // NCPA's `name` for a process is the executable's name, and its `exe`
      // is the executable with its path.
      entry.name = row[0];
      entry.exe = row[1].empty() ? row[0] : row[1];
      entry.username = row[2];
      entry.pid = str::stox<long long>(row[3], 0);
      entry.cmd = row[4];
      out.push_back(entry);
    }
    return out;
  }

 private:
  struct inventory_entry {
    std::string name;
    // The module that registered it, as the core names it: the instance alias
    // when the operator configured one, otherwise the module's file name.
    std::string plugin;
  };

  // Every registered query, with the module that registered it.
  std::vector<inventory_entry> inventory() {
    std::vector<inventory_entry> out;
    PB::Registry::RegistryRequestMessage request;
    PB::Registry::RegistryRequestMessage::Request *payload = request.add_payload();
    // Never fetch_all: that makes the core run every registered command with
    // `help-pb` to collect its parameters, which for a listing means running
    // every check on the box.
    payload->mutable_inventory()->set_fetch_all(false);
    payload->mutable_inventory()->add_type(PB::Registry::ItemType::QUERY);
    std::string buffer;
    core_->registry_query(request.SerializeAsString(), buffer);

    PB::Registry::RegistryResponseMessage response;
    response.ParseFromString(buffer);
    for (const PB::Registry::RegistryResponseMessage::Response &r : response.payload()) {
      for (const PB::Registry::RegistryResponseMessage::Response::Inventory &i : r.inventory()) {
        inventory_entry entry;
        entry.name = i.name();
        if (i.info().plugin_size() > 0) entry.plugin = i.info().plugin(0);
        out.push_back(entry);
      }
    }
    return out;
  }

  // Whether a query came from an external-scripts module. The core reports the
  // instance alias when one is configured, so both the module's own name and
  // its declared alias count; an operator who renamed the instance to something
  // else gets nothing exposed rather than everything, and can name the commands
  // explicitly instead.
  static bool is_external_script(const inventory_entry &entry) {
    return boost::iequals(entry.plugin, "CheckExternalScripts") || boost::iequals(entry.plugin, "ext-script");
  }

  // Whether a platform's state word means the service is up. Everything else,
  // including a state the agent could not determine, counts as stopped - which
  // is what makes a check on it alert rather than quietly pass.
  static bool is_running(const std::string &state) {
    return boost::iequals(state, "running") || boost::iequals(state, "started") || boost::iequals(state, "active");
  }

  // Run a filter check purely to enumerate, and split its output back into
  // rows. The check renders one record per line with tab-separated fields; a
  // field that can contain a tab has to be the last one asked for, because the
  // split keeps the remainder in it.
  std::vector<std::vector<std::string> > tabulate(const std::string &command, const std::vector<std::string> &fields,
                                                  const std::vector<std::string> &extra_arguments = {}) {
    std::vector<std::vector<std::string> > rows;

    std::string detail;
    for (const std::string &field : fields) {
      if (!detail.empty()) detail += "\t";
      detail += "%(" + field + ")";
    }
    std::list<std::string> arguments{"filter=none",        "warning=none",   "critical=none", "top-syntax=%(list)", "detail-syntax=" + detail,
                                     "list-separator=\\n", "empty-state=ok", "empty-syntax="};
    arguments.insert(arguments.end(), extra_arguments.begin(), extra_arguments.end());

    std::string message;
    std::string perf;
    nscapi::core_helper(core_, plugin_id_).simple_query(command, arguments, message, perf, nscapi::protobuf::functions::no_truncation);

    for (const std::string &line : str::utils::split_lst(message, std::string("\n"))) {
      if (line.empty()) continue;
      std::vector<std::string> row;
      std::size_t start = 0;
      // fields.size() - 1 splits, so the last field keeps any tab of its own.
      for (std::size_t i = 0; i + 1 < fields.size(); ++i) {
        const std::size_t tab = line.find('\t', start);
        if (tab == std::string::npos) break;
        row.push_back(line.substr(start, tab - start));
        start = tab + 1;
      }
      if (row.size() != fields.size() - 1) continue;
      row.push_back(line.substr(start));
      rows.push_back(row);
    }
    return rows;
  }

  const nscapi::core_wrapper *core_;
  unsigned int plugin_id_;
  ncpa::session_ptr session_;
};

}  // namespace

NCPAServer::NCPAServer()
    : session_(std::make_shared<ncpa::session>()), snapshot_(std::make_shared<ncpa::metrics_snapshot>()), deltas_(std::make_shared<ncpa::delta_store>()) {}

NCPAServer::~NCPAServer() = default;

bool NCPAServer::loadModuleEx(std::string alias, const NSCAPI::moduleLoadMode mode) {
  // A settings reload re-enters this on the live module while the HTTP poll
  // thread is serving requests, so the old listener has to be gone before a
  // new one binds the port.
  try {
    if (server_) {
      server_->stop();
      server_.reset();
    }
  } catch (...) {
    NSC_LOG_ERROR_EX("Failed to stop the running NCPA server");
    return false;
  }

  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("NCPA", std::move(alias), "server");

  std::string port;
  std::string certificate;
  std::string key;
  bool allow_insecure = false;
  bool log_errors = true;
  bool log_info = false;
  bool log_debug = false;

  settings.alias().add_path_to_settings()("NCPA server", "Section for the NCPA server (check_ncpa) protocol options.")(
      "log", "Log configuration", "Configure which messages from the NCPA server are logged.");

  // clang-format off
  settings.alias()
      .add_key_to_settings()
      .add_string("port", sh::string_key(&port, "5693"), "PORT NUMBER",
                  "Port to listen on. 5693 is the port check_ncpa.py and the Nagios XI NCPA wizard default to.")
      .add_password("token", nscapi::settings_helper::string_fun_key([this](auto value) { this->session_->set_token(value); }),
                    "NCPA TOKEN",
                    "The shared secret every request must carry as `?token=...`, which check_ncpa.py sends with `-t`. There is no default: until one is "
                    "set the server answers every request with NCPA's 'Incorrect credentials given.' error, so an agent that boots with this module "
                    "enabled but unconfigured exposes nothing.")
      .add_password("backup token", nscapi::settings_helper::string_fun_key([this](auto value) { this->session_->set_backup_token(value); }),
                    "NCPA BACKUP TOKEN",
                    "A second accepted token. Set it to the new secret while the monitoring servers are being moved over, then swap it into `token` and "
                    "clear this - which is how a token is rotated without a window where checks fail.")
      .add_bool("allow arguments", nscapi::settings_helper::bool_fun_key([this](auto value) { this->session_->set_allow_arguments(value); }, false),
                "COMMAND ARGUMENT PROCESSING",
                "Whether a caller may put arguments on a `plugins/` path (`check_ncpa -M plugins/check_cpu -a 'warning=load>80'`). False by default, the "
                "same default and the same reasoning as the NRPE server's option of this name: a caller that can shape a check's arguments can ask the "
                "agent rather more than the operator meant to offer.")
      .add_string("plugins", nscapi::settings_helper::string_fun_key([this](auto value) { this->session_->set_exposed_plugins(value); }, "any"),
                  "WHICH QUERIES plugins/ EXPOSES",
                  "`any` (the default) exposes every registered query by name, which is what makes every NSClient++ check reachable from Nagios with no "
                  "mapping work. `scripts` exposes only the external scripts configured on this agent, not its built-in check_* commands. Anything else "
                  "is read as a comma-separated list of query names, and nothing outside it is callable.")
      .add_string("default units", nscapi::settings_helper::string_fun_key([this](auto value) { this->session_->set_default_units(value); }, ""),
                  "DEFAULT UNIT PREFIX",
                  "The `units` prefix to apply when a request sends none (k, Ki, M, Mi, G, Gi, T, Ti). NCPA's [general] default_units. Only byte-valued "
                  "nodes are rescaled, so this never changes a percentage or a count.")
      .add_bool("expose version", nscapi::settings_helper::bool_fun_key([this](auto value) { this->session_->set_expose_version(value); }, true),
                "EXPOSE VERSION IN system/agent_version",
                "Whether `system/agent_version` reports the real NSClient++ version. True by default, because the Nagios XI wizard reads it. Set to false "
                "to answer a placeholder instead; the node still exists, so discovery keeps working.")
      .add_bool("allow insecure", sh::bool_key(&allow_insecure, false), "ALLOW INSECURE (CLEARTEXT HTTP)",
                "When false (the default) the server refuses to start if the TLS certificate is missing, rather than serving the API - and the token that "
                "opens it - in clear. Set to true to explicitly accept unencrypted HTTP; check_ncpa.py speaks https:// only, so this is only useful "
                "behind a TLS-terminating proxy.")
      .add_int("auth rate limit max failures",
               nscapi::settings_helper::int_fun_key([this](auto value) { this->session_->rate_limiter().set_max_failures(value); },
                                                    auth_rate_limiter::kDefaultMaxFailures),
               "AUTH RATE LIMIT (FAILURES)",
               "How many consecutive bad tokens from one client IP trigger a block. Default 10. Set to 0 to disable the limiter.")
      .add_int("auth rate limit block seconds",
               nscapi::settings_helper::int_fun_key([this](auto value) { this->session_->rate_limiter().set_block_seconds(value); },
                                                    auth_rate_limiter::kDefaultBlockSeconds),
               "AUTH RATE LIMIT (BLOCK SECONDS)",
               "How long a blocked IP stays blocked. Default 60 s, doubling up to an hour for a client that burns its whole budget at machine speed.")
      .add_string("certificate", sh::string_key(&certificate, "${certificate-path}/certificate.pem"), "TLS CERTIFICATE",
                  "The certificate to serve. Defaults to the same path the web server uses, so one certificate serves both. check_ncpa.py does not verify "
                  "it unless it is called with `-s`, so a self-signed certificate works out of the box.")
      .add_string("certificate key", sh::string_key(&key), "TLS PRIVATE KEY", "The private key, when it is not in the certificate file.");

  settings.alias()
      .add_key_to_settings("log")
      .add_bool("error", sh::bool_key(&log_errors, true), "Log errors", "Enable logging of errors from the NCPA server.")
      .add_bool("info", sh::bool_key(&log_info, false), "Log info", "Enable logging of info messages from the NCPA server.")
      .add_bool("debug", sh::bool_key(&log_debug, false), "Log debug", "Enable logging of debug messages from the NCPA server.");

  settings.alias()
      .add_parent("/settings/default")
      .add_key_to_settings()
      .add_string("allowed hosts", nscapi::settings_helper::string_fun_key([this](auto value) { this->session_->set_allowed_hosts(value); }, "127.0.0.1"),
                  "ALLOWED HOSTS", "A comma separated list of allowed hosts. Netmasks (/ syntax) and * ranges are accepted.")
      .add_bool("cache allowed hosts",
                nscapi::settings_helper::bool_fun_key([this](auto value) { this->session_->set_allowed_hosts_cache(value); }, true),
                "CACHE THE ALLOWED HOSTS LIST",
                "Whether resolved host names are cached. Faster, but a monitoring server on a dynamic address then needs a reload to be recognised.");
  // clang-format on

  settings.register_all();
  settings.notify();

  certificate = get_core()->expand_path(certificate);
  key = get_core()->expand_path(key);

  std::list<std::string> errors = session_->boot();
  NSC_LOG_ERROR_LISTS(errors);

  if (mode != NSCAPI::normalStart && mode != NSCAPI::reloadStart) return true;

  if (!session_->has_token()) {
    NSC_LOG_ERROR(
        "No NCPA token is configured: the NCPA server will answer every request with 'Incorrect credentials given.'. Set 'token' under "
        "/settings/NCPA/server to the shared secret the monitoring server passes to check_ncpa with -t.");
  }

  socket_helpers::validate_certificate(certificate, errors);
  NSC_LOG_ERROR_LISTS(errors);

  const bool certificate_missing = !boost::filesystem::is_regular_file(certificate);
  if (certificate_missing && !allow_insecure) {
    NSC_LOG_ERROR("NCPA certificate not found at '" + certificate +
                  "': refusing to start the NCPA server in cleartext HTTP, which would put the token on the wire in clear on every check. Provide a "
                  "certificate, or set 'allow insecure = true' to accept that explicitly. The NCPA server has NOT been started.");
    return true;
  }

  // The dispatcher outlives every request but reads the session, which a reload
  // rewrites in place - so it is rebuilt here only because the plugin id is not
  // known before the first load.
  dispatcher_ = std::make_shared<core_dispatcher>(get_core(), get_id(), session_);

  ncpa::tree_options options;
  options.agent_version = get_core()->getApplicationVersionString();

  const Mongoose::WebLoggerPtr logger(new ncpa_logger(log_errors, log_info, log_debug));
  server_.reset(Mongoose::Server::make_server(logger));
  if (certificate_missing) {
    NSC_LOG_ERROR("NCPA certificate not found at '" + certificate + "' and 'allow insecure = true' is set: serving UNENCRYPTED HTTP on port " + port +
                  ". The token travels in clear on every check - only use this behind a TLS-terminating proxy.");
  } else {
    NSC_DEBUG_MSG("Using certificate: " + certificate);
    server_->setSsl(certificate, key);
  }

  // Registered before start(): the mongoose backend serves on a single poll
  // thread and races a controller registered afterwards.
  server_->registerController(new ncpa::controller(session_, snapshot_.get(), dispatcher_.get(), deltas_.get(), options));

  try {
    server_->start("0.0.0.0:" + port);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR("Failed to start the NCPA server: " + utf8::utf8_from_native(e.what()));
    return true;
  } catch (const std::string &e) {
    NSC_LOG_ERROR("Failed to start the NCPA server: " + e);
    return true;
  }
  NSC_DEBUG_MSG("Loading NCPA server on port: " + port);
  return true;
}

void NCPAServer::prepareShutdown() {
  // Stop the HTTP poll thread (which closes the listening socket) while every
  // peer plugin is still loaded, so a request already inside `plugins/` can
  // finish its query before unloadModule runs.
  try {
    if (server_) server_->stop();
  } catch (...) {
    NSC_LOG_ERROR_EX("prepare_shutdown");
  }
}

bool NCPAServer::unloadModule() {
  try {
    if (server_) {
      server_->stop();
      server_.reset();
    }
  } catch (...) {
    NSC_LOG_ERROR_EX("unload");
    return false;
  }
  return true;
}

void NCPAServer::submitMetrics(const PB::Metrics::MetricsMessage &message) const { snapshot_->set(message); }
