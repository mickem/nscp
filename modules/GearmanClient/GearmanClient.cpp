// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include "GearmanClient.h"

#include <boost/algorithm/string.hpp>
#include <boost/asio/ip/host_name.hpp>
#include <boost/thread.hpp>
#include <ctime>
#include <fstream>
#include <list>
#include <memory>
#include <net/socket/socket_helpers.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_core_helper.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/protobuf/functions_query.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/proxy.hpp>
#include <str/utf8.hpp>
#include <str/utils.hpp>
#include <str/xtos.hpp>
#include <vector>

#include "gearman_client.hpp"
#include "gearman_handler.hpp"

#ifndef WIN32
#include <sys/stat.h>
#endif

namespace sh = nscapi::settings_helper;

namespace {

/**
 * The plugin output a result carries. Long enough for a detail-syntax line
 * with performance data; mod_gearman's own workers read a plugin's stdout
 * with no limit at all, but the core truncates on its side anyway and an
 * unbounded string here would put a runaway check's output on the wire.
 */
const std::size_t max_output_length = 8 * 1024;

/** Route the worker's log lines through the core, which is the only reason this exists. */
class core_logger : public gearman::worker_logger {
 public:
  void error(const std::string &message) override { NSC_LOG_ERROR_STD("gearman: " + message); }
  void warning(const std::string &message) override { NSC_LOG_MESSAGE("gearman: " + message); }
  void info(const std::string &message) override { NSC_DEBUG_MSG("gearman: " + message); }
  void debug(const std::string &message) override { NSC_TRACE_MSG("gearman: " + message); }
};

/**
 * State a check shares with the thread running it. Held through a shared_ptr
 * by both, so a check that overruns its timeout keeps writing into live heap
 * rather than into the worker's stack frame - the same arrangement
 * CheckHelpers uses for check_timeout, and for the same reason.
 */
struct query_state {
  void run(nscapi::core_wrapper *core, const int plugin_id, const std::string &command, const std::list<std::string> &arguments) {
    nscapi::core_helper helper(core, plugin_id);
    // The module's own id as the caller: a job carries no upstream identity
    // (gearmand does not authenticate anybody), so the permission policy sees
    // the GearmanClient module and nothing more specific.
    ok = helper.simple_query_on_behalf_of(str::xtos(plugin_id), "", command, arguments, response);
  }
  bool ok = false;
  std::string response;
};

/** Runs a job's command line as a native NSClient++ query. */
class core_query_executor : public gearman::query_executor {
 public:
  core_query_executor(nscapi::core_wrapper *core, const int plugin_id) : core_(core), plugin_id_(plugin_id) {}

  gearman::query_result execute(const std::string &command, const std::list<std::string> &arguments, const unsigned int timeout_seconds) override {
    gearman::query_result result;
    const std::shared_ptr<query_state> state = std::make_shared<query_state>();
    nscapi::core_wrapper *core = core_;
    const int plugin_id = plugin_id_;
    const std::shared_ptr<boost::thread> runner =
        std::make_shared<boost::thread>([state, core, plugin_id, command, arguments] { state->run(core, plugin_id, command, arguments); });

    if (!runner->timed_join(boost::posix_time::seconds(timeout_seconds))) {
      // A check cannot be cancelled, so the thread is left to finish and its
      // answer is dropped; the core gets the timeout result instead.
      park(runner);
      result.timed_out = true;
      return result;
    }
    if (!state->ok) {
      result.return_code = 3;
      result.output = "Command was not found: " + command;
      return result;
    }

    std::string message, perf;
    result.return_code = nscapi::protobuf::functions::parse_simple_query_response(state->response, message, perf, max_output_length);
    result.output = perf.empty() ? message : message + "|" + perf;
    return result;
  }

  /**
   * Give the overrunning checks a bounded wait. A boost::thread object whose
   * thread is still running detaches when it is destroyed, so the list must
   * not simply be dropped while the module is being unloaded.
   */
  ~core_query_executor() override {
    boost::lock_guard<boost::mutex> lock(mutex_);
    for (const std::shared_ptr<boost::thread> &thread : parked_) {
      try {
        thread->timed_join(boost::posix_time::seconds(30));
      } catch (...) {
      }
    }
  }

 private:
  void park(const std::shared_ptr<boost::thread> &thread) {
    boost::lock_guard<boost::mutex> lock(mutex_);
    for (auto it = parked_.begin(); it != parked_.end();) {
      it = (*it)->timed_join(boost::posix_time::seconds(0)) ? parked_.erase(it) : ++it;
    }
    parked_.push_back(thread);
  }

  nscapi::core_wrapper *const core_;
  const int plugin_id_;
  boost::mutex mutex_;
  std::list<std::shared_ptr<boost::thread>> parked_;
};

}  // namespace

GearmanClient::GearmanClient()
    : client_("gearman", std::make_shared<gearman_client::gearman_client_handler>(), std::make_shared<gearman_handler::options_reader_impl>()) {}
GearmanClient::~GearmanClient() = default;

GearmanClient::worker_setup GearmanClient::build_config(const std::string &alias, gearman::worker_config &config) {
  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("gearman", alias, "worker");

  std::string servers, mode_name = "agent", key, key_file, hostgroups, servicegroups, host_names;
  bool encryption = true, insecure = false, shared_queues = false;
  unsigned int workers = 2;
  int timeout_return = 2;
  int max_age = 0;

  settings.alias().add_path_to_settings()("Gearman worker", "Section for the Mod-Gearman worker (GearmanClient.dll).");

  // clang-format off
  settings.alias().add_key_to_settings()
    .add_string("server", sh::string_key(&servers, ""), "GEARMAND SERVERS",
        "Comma separated list of gearmand job servers as host or host:port (port defaults to 4730), tried in order. "
        "This is the same list mod_gearman's worker.conf gives as repeated server= lines. The agent always connects outbound, "
        "so no port is opened on this host.")

    .add_string("mode", sh::string_key(&mode_name, "agent"), "WORKER MODE",
        "Which deployment this is, 'agent' or 'proxy'. An agent answers for itself: it runs the checks of the host it is installed on, and refuses a job "
        "for any other host (see 'host names'). A proxy answers for others: it runs every check on the queues it registered, whichever host the core meant "
        "it for, which is what a check_command naming its own target - check_nrpe host=$HOSTADDRESS$ command=check_cpu, check_wmi target=$HOSTADDRESS$ - "
        "needs (the agent reads a check's arguments as key=value or --long, not as the Nagios plugin's -H). Proxy mode is "
        "how one domain-joined Windows box monitors a whole hostgroup without an agent, or an open port, on any of them; it is also the bigger target, since "
        "anyone who can queue a job on those queues reaches everything the proxy's own credentials reach.")

    .add_bool("encryption", sh::bool_key(&encryption, true), "ENCRYPT PAYLOADS",
        "Whether jobs and results travel inside the AES-256 envelope (mod_gearman's encryption=yes). Leave this on: with it off "
        "the payloads are plain base64 and anyone who can reach gearmand can read and forge checks. Turning it off also requires "
        "'insecure = true'.")

    .add_bool("insecure", sh::bool_key(&insecure, false), "ALLOW UNENCRYPTED PAYLOADS",
        "Acknowledge that 'encryption = false' sends and accepts check jobs with no protection at all. Without this the module "
        "refuses to start unencrypted.")

    .add_password("key", sh::string_key(&key, ""), "SHARED KEY",
        "The shared password from the core's module.conf (key=). At most 32 bytes are used, as in mod_gearman. Use 'key file' "
        "instead to keep it out of the configuration file.")

    .add_string("key file", sh::path_key(&key_file, ""), "SHARED KEY FILE",
        "Path to a file whose first line is the shared key, like mod_gearman's keyfile=. Takes effect only when 'key' is empty. "
        "Restrict it to the account the agent runs as: anyone who can read it can inject checks into every queue this agent serves.")

    .add_string("hostgroups", sh::string_key(&hostgroups, ""), "HOSTGROUP QUEUES",
        "Comma separated hostgroup names, one queue each: 'windows' registers hostgroup_windows. These have to match the "
        "hostgroups= line in the core's module.conf, which is what decides that a check goes to gearmand at all. In agent mode "
        "the usual arrangement is one hostgroup per host; a proxy takes one group for every host it monitors, and two proxies on the same group share "
        "the load and cover each other with no further configuration.")

    .add_string("servicegroups", sh::string_key(&servicegroups, ""), "SERVICEGROUP QUEUES",
        "Comma separated servicegroup names, one queue each: 'db' registers servicegroup_db. Matches servicegroups= in the "
        "core's module.conf.")

    .add_bool("allow shared queues", sh::bool_key(&shared_queues, false), "ANSWER THE GENERIC QUEUES",
        "Also register the generic 'host' and 'service' queues that every check without a group lands on. Off by default "
        "because a Windows agent registering there grabs checks meant for every other host in the installation, including the "
        "Linux ones.")

    .add_string("host names", sh::string_key(&host_names, ""), "EXTRA HOST NAMES",
        "Comma separated extra names this agent answers for, on top of its own host name. A queue carries the checks of every "
        "host in its group and the protocol does not say which worker a job was meant for, so a job for any other name is "
        "refused. Set this when the core knows the host under a different name than the operating system does. Agent mode only: "
        "a proxy answers for every host on its queues and ignores this.")

    .add_int("workers", sh::uint_key(&workers, 2), "WORKER THREADS",
        "Number of worker threads, each with its own connection. Two is plenty for one host's own checks.")

    .add_int("timeout return", sh::int_key(&timeout_return, 2), "STATUS ON TIMEOUT",
        "The status reported when a check does not finish inside the timeout the core put in the job: 0 ok, 1 warning, "
        "2 critical, 3 unknown. Same meaning as mod_gearman's timeout_return.")

    .add_int("max age", sh::int_key(&max_age, 0), "DISCARD JOBS OLDER THAN",
        "Refuse a job whose core_time is more than this many seconds in the past, answering unknown instead of running it "
        "(0 disables). After an outage the queue holds a backlog of checks whose answers describe a moment that has passed.")

    .add_bool("allow arguments", sh::bool_key(&config.allow_arguments, true), "COMMAND ARGUMENT PROCESSING",
        "Whether a job may carry arguments. On by default, unlike NRPE: the core has already expanded $ARGn$ before the job "
        "was queued, so a check_command defined on the core is nothing but arguments and turning this off leaves only "
        "bare commands.")

    .add_bool("allow nasty characters", sh::bool_key(&config.allow_nasty_characters, false), "COMMAND ALLOW NASTY META CHARS",
        "Whether a job may contain nasty (as in |`&><'\"\\[]{}) characters. Same guard and same default as NRPEServer. Note that this "
        "rejects a threshold written the Nagios way, 'warn=load>80', because '>' is in the set: write it as 'warn=load gt 80' in the core's "
        "check_command, which the filter language understands and which needs no exception here, or turn this on if the command lines cannot "
        "be changed.")
    ;
  // clang-format on

  settings.register_all();
  settings.notify();

  const std::vector<std::string> groups = str::utils::split_trimmed(hostgroups, ",");
  const std::vector<std::string> service_groups = str::utils::split_trimmed(servicegroups, ",");

  if (servers.empty() && groups.empty() && service_groups.empty() && !shared_queues) {
    // Nothing in the worker section at all. That is a deployment, not a
    // mistake: the module is also the passive channel below, and an
    // installation replacing NSCA with Mod-Gearman configures only that half.
    NSC_DEBUG_MSG("gearman: no worker configured; the module is loaded for its submit channel only.");
    return worker_setup::disabled;
  }
  if (servers.empty()) {
    NSC_LOG_ERROR_STD("gearman: no job server configured. Set /settings/gearman/worker/server to the gearmand the core submits to.");
    return worker_setup::invalid;
  }
  try {
    config.servers = gearman::parse_server_list(servers);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_STD(std::string("gearman: could not read the server list: ") + e.what());
    return worker_setup::invalid;
  }
  if (config.servers.empty()) {
    NSC_LOG_ERROR_STD("gearman: the server list is empty.");
    return worker_setup::invalid;
  }

  if (key.empty() && !key_file.empty()) {
    const std::string path = get_core()->expand_path(key_file);
    std::ifstream stream(path.c_str());
    if (!stream) {
      NSC_LOG_ERROR_STD("gearman: could not read the key file: " + path);
      return worker_setup::invalid;
    }
    std::getline(stream, key);
    boost::trim_right(key);
#ifndef WIN32
    // A key file readable by anyone on the box is as good as no key: whoever
    // reads it can queue a job for every queue this agent answers. The
    // equivalent ACL check on Windows is not done here, so the documentation
    // is what tells a Windows operator to restrict the file.
    struct stat info = {};
    if (::stat(path.c_str(), &info) == 0 && (info.st_mode & (S_IRGRP | S_IROTH)) != 0) {
      NSC_LOG_MESSAGE("gearman: the key file " + path + " is readable by more than its owner; anyone who can read it can inject checks.");
    }
#endif
  }

  config.crypto.encryption = encryption;
  config.crypto.key = key;
  if (encryption && key.empty()) {
    NSC_LOG_ERROR_STD(
        "gearman: encryption is on but no key is set. The key is the only thing separating a check the core scheduled from one anybody who can reach "
        "gearmand made up, so the module will not start without it. Set 'key' (or 'key file') to the same value as the core's module.conf.");
    return worker_setup::invalid;
  }
  if (!encryption && !insecure) {
    NSC_LOG_ERROR_STD(
        "gearman: encryption is off but 'insecure' is not set. Unencrypted payloads let anyone who can reach gearmand read and forge this host's checks; "
        "set 'insecure = true' to say that is intended.");
    return worker_setup::invalid;
  }
  if (!encryption) {
    NSC_LOG_ERROR_STD("gearman: running with encryption disabled. Check jobs and results are plain base64 on the wire and are neither secret nor verified.");
  }

  for (const std::string &group : groups) config.queues.push_back("hostgroup_" + group);
  for (const std::string &group : service_groups) config.queues.push_back("servicegroup_" + group);
  if (shared_queues) {
    config.queues.emplace_back("host");
    config.queues.emplace_back("service");
  }
  if (config.queues.empty()) {
    NSC_LOG_ERROR_STD(
        "gearman: no queue to answer. Set 'hostgroups' (and/or 'servicegroups') to the groups the core's module.conf routes through gearmand; a worker with "
        "no registered queue never receives a check.");
    return worker_setup::invalid;
  }

  const std::string local_host = boost::asio::ip::host_name();
  config.host_names.insert(boost::algorithm::to_lower_copy(local_host));
  // The short name too: a core commonly knows a host as `win-srv01` where the
  // OS answers `win-srv01.example.com`, and refusing that job would look like
  // the agent being down.
  const std::string::size_type dot = local_host.find('.');
  if (dot != std::string::npos) config.host_names.insert(boost::algorithm::to_lower_copy(local_host.substr(0, dot)));
  for (const std::string &name : str::utils::split_trimmed(host_names, ",")) config.host_names.insert(boost::algorithm::to_lower_copy(name));

  const std::string mode = boost::algorithm::to_lower_copy(boost::trim_copy(mode_name));
  if (mode == "agent") {
    config.bind_to_host = true;
  } else if (mode == "proxy") {
    config.bind_to_host = false;
    // Worth one line in the log: from here on this agent runs whatever the
    // queue carries, for whatever host, so the queue and its key are the only
    // thing between a job and everything this host can reach.
    NSC_LOG_MESSAGE("gearman: running in proxy mode: every check on " + str::utils::joinEx(config.queues, ", ") +
                    " is executed here whichever host it names, through this agent's own commands and credentials. Keep the key to these queues to the "
                    "hosts that should have it.");
    if (!str::utils::split_trimmed(host_names, ",").empty())
      NSC_LOG_MESSAGE("gearman: 'host names' is set but has no effect in proxy mode: a proxy answers for every host on its queues, which is the point of it.");
  } else {
    NSC_LOG_ERROR_STD("gearman: unknown mode '" + mode_name +
                      "'. Use 'agent' (this host's own checks only, the default) or 'proxy' (run the checks of every host on the registered queues).");
    return worker_setup::invalid;
  }

  config.workers = workers;
  config.timeout_return = timeout_return;
  config.max_age = max_age;
  config.client_id = "nscp-" + local_host;
  config.source = "NSClient++ " + utf8::cvt<std::string>(get_core()->getApplicationVersionString()) + " on " + local_host;
  return worker_setup::ready;
}

void GearmanClient::build_client(const std::string &alias) {
  sh::settings_registry settings(nscapi::settings_proxy::create(get_id(), get_core()));
  settings.set_alias("gearman", alias, "client");
  client_.set_path(settings.alias().get_settings_path("targets"));

  // clang-format off
  settings.alias().add_path_to_settings()
    ("Gearman submit channel", "Section for submitting passive results into a Mod-Gearman result queue (GearmanClient.dll).")

    ("handlers", sh::fun_values_path([this](auto key, auto value) { this->add_command(key, value); }),
        "CLIENT HANDLER SECTION", "",
        "CLIENT HANDLER", "For more configuration options add a dedicated section")

    ("targets", sh::fun_values_path([this](auto key, auto value) { this->add_target(key, value); }),
        "REMOTE TARGET DEFINITIONS", "",
        "TARGET", "For more configuration options add a dedicated section")
    ;
  // clang-format on

  settings.alias()
      .add_key_to_settings()

      .add_string("hostname", sh::string_key(&hostname_, "auto"), "HOSTNAME",
                  "The host name results are filed under on the core, which has to be the name the core knows this host by. Set this to auto (default) to "
                  "use the name of this computer.\n\n"
                  "auto\tHostname\n"
                  "${host}\tHostname\n"
                  "${host_lc}\tHostname in lowercase\n"
                  "${host_uc}\tHostname in uppercase\n"
                  "${domain}\tDomainname\n"
                  "${domain_lc}\tDomainname in lowercase\n"
                  "${domain_uc}\tDomainname in uppercase\n"
                  "${address_ipv4}\tIPv4 address of the computer\n"
                  "${address_ipv6}\tIPv6 address of the computer (lowercase, compressed)\n")

      .add_string("channel", sh::string_key(&channel_, "GEARMAN"), "CHANNEL",
                  "The channel to listen to. A Scheduler entry (or any other submitting module) naming this channel has its results pushed into the "
                  "target's result queue, which is how this module replaces NSCA in a Mod-Gearman installation.");

  settings.register_all();
  settings.notify();

  client_.finalize(nscapi::settings_proxy::create(get_id(), get_core()));

  nscapi::core_helper core(get_core(), get_id());
  core.register_channel(channel_);

  hostname_ = socket_helpers::expand_hostname(hostname_);
  client_.set_sender(hostname_);
}

void GearmanClient::add_target(const std::string &key, const std::string &args) {
  try {
    client_.add_target(nscapi::settings_proxy::create(get_id(), get_core()), key, args);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("gearman: failed to add target: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("gearman: failed to add target: " + key);
  }
}

void GearmanClient::add_command(const std::string &key, const std::string &args) {
  try {
    nscapi::core_helper core(get_core(), get_id());
    const std::string command = client_.add_command(key, args);
    if (!command.empty()) core.register_command(command.c_str(), "Gearman relay for: " + key);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("gearman: failed to add command: " + key, e);
  } catch (...) {
    NSC_LOG_ERROR_EX("gearman: failed to add command: " + key);
  }
}

bool GearmanClient::loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode) {
  // A reload calls this again on the live module while the previous workers
  // are still grabbing jobs, so they go first - otherwise each reload leaves
  // another set of them registered on the same queue. The submit channel's
  // targets and relay commands are rebuilt from scratch for the same reason:
  // the settings callbacks in build_client() append, so a reload that did not
  // clear them first would duplicate every entry.
  if (!pool_.stop()) NSC_LOG_MESSAGE("gearman: a check was still running when the workers were stopped; it will finish on its own.");
  client_.clear();

  try {
    build_client(alias);
  } catch (const std::exception &e) {
    NSC_LOG_ERROR_EXR("gearman: failed to read the submit channel configuration", e);
    return false;
  }

  gearman::worker_config config;
  const worker_setup setup = build_config(alias, config);
  // The two halves are independent deployments, so a worker section that
  // cannot be used does not take the submit channel down with it: the reason
  // has already been logged as an error, and an installation using this module
  // only to replace NSCA keeps working.
  if (setup != worker_setup::ready) return true;

  if (mode != NSCAPI::normalStart && mode != NSCAPI::reloadStart) return true;

  const std::shared_ptr<gearman::worker_logger> logger = std::make_shared<core_logger>();
  const std::shared_ptr<gearman::query_executor> executor = std::make_shared<core_query_executor>(get_core(), get_id());
  pool_.start(config, executor, logger);
  NSC_DEBUG_MSG("gearman: started " + str::xtos(config.workers) + " worker(s) for " + str::xtos(static_cast<unsigned int>(config.queues.size())) + " queue(s)");
  return true;
}

bool GearmanClient::unloadModule() {
  if (!pool_.stop()) NSC_LOG_MESSAGE("gearman: a check outlived the shutdown wait; it will finish on its own.");
  client_.clear();
  return true;
}

void GearmanClient::handleNotification(const std::string &, const PB::Commands::SubmitRequestMessage &request_message,
                                       PB::Commands::SubmitResponseMessage *response_message) {
  client_.do_submit(request_message, *response_message);
}

bool GearmanClient::commandLineExec(const int target_mode, const PB::Commands::ExecuteRequestMessage &request, PB::Commands::ExecuteResponseMessage &response) {
  if (target_mode == NSCAPI::target_module) {
    return client_.do_exec(request, response, "submit_");
  }
  return false;
}

void GearmanClient::query_fallback(const PB::Commands::QueryRequestMessage &request_message, PB::Commands::QueryResponseMessage &response_message) {
  client_.do_query(request_message, response_message);
}

void GearmanClient::fetchMetrics(PB::Metrics::MetricsMessage::Response *response) {
  using nscapi::metrics::describe;
  using nscapi::metrics::metric;

  PB::Metrics::MetricsBundle *bundle = response->add_bundles();
  bundle->set_key("gearman");
  describe(bundle, "The Mod-Gearman worker pool: what it has taken off the queues and whether it is still there to take more");

  const gearman::worker_counters &counters = pool_.counters();
  // Jobs and errors only ever grow for the lifetime of the agent, so
  // `rate(gearman_worker_jobs_total[5m])` is the checks per second this agent
  // actually answers. The other two describe the pool right now and can go
  // back down, which is what makes them gauges.
  metric(bundle, "worker.jobs").help("Check jobs taken off a queue since the agent was started").counter(counters.jobs.load());
  metric(bundle, "worker.errors")
      .help("Connection failures, undecodable payloads and results that could not be submitted, since the agent was started")
      .counter(counters.errors.load());
  metric(bundle, "worker.connected").help("Worker threads currently holding a live connection to a job server").gauge(counters.connected.load());

  // Seconds since the last job was grabbed, which is what an operator watches:
  // a worker that is connected but has not been given a check in an hour means
  // the core stopped routing, not that the agent is down. -1 rather than 0 for
  // "no job yet": a zero would read as a check having just arrived.
  const long long last = counters.last_job_time.load();
  const long long age = last == 0 ? -1 : static_cast<long long>(std::time(nullptr)) - last;
  metric(bundle, "worker.last_job_age").help("Seconds since the last check job was grabbed, or -1 if none has been").unit("seconds").gauge(age);
}
