// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <config.h>

#include <boost/filesystem/operations.hpp>
#include <boost/unordered_set.hpp>
#include <nscapi/nscapi_metrics_helper.hpp>
#include <nscapi/settings/helper.hpp>
#include <nsclient/logger/logger_helper.hpp>
#include <settings/settings_core.hpp>

#include "../libs/settings_manager/settings_manager_impl.h"
#include "NSClient++.h"
#include "agent_facts.hpp"
#include "cli_parser.hpp"
#include "core_api.h"
#include "core_settings.hpp"
#include "diagnostics_round.hpp"
#include "fatal_handler.hpp"
#include "logger/nsclient_logger.hpp"
#include "reload_request.hpp"
#ifdef HAVE_ONBOARDING
#include <net/socket/socket_helpers.hpp>
#include <str/format.hpp>

#include "fleet_settings.hpp"
#include "fleet_sync.hpp"
#endif
#ifdef WIN32
#include "shared_folder.hpp"
#include "windows_ca_store.hpp"
#endif

#ifdef WIN32
#include <win/com_helpers.hpp>
#include <win/service_control.hpp>
com_helper::initialize_com com_helper_;
#endif

#ifdef WIN32
#include <breakpad/exception_handler_win32.hpp>
#endif

std::shared_ptr<NSClient> mainClient;  // Global core instance.

/**
 * Application startup point
 *
 * @param argc Argument count
 * @param argv[] Argument array
 * @param envp[] Environment array
 * @return exit status
 */
int nscp_main(int argc, char *argv[]);

#ifdef WIN32
int wmain(int argc, wchar_t *argv[], wchar_t *envp[]) {
  char **wargv = new char *[argc];
  for (int i = 0; i < argc; i++) {
    std::string s = utf8::cvt<std::string>(argv[i]);
    wargv[i] = new char[s.length() + 10];
    strncpy(wargv[i], s.c_str(), s.size() + 1);
  }
  int ret = nscp_main(argc, wargv);
  for (int i = 0; i < argc; i++) {
    delete[] wargv[i];
  }
  delete[] wargv;
  return ret;
}
#else
int main(int argc, char *argv[]) { return nscp_main(argc, argv); }
#endif

int nscp_main(int argc, char *argv[]) {
  // First thing, before anything can throw: the try below only covers this
  // thread, and only exceptions that actually unwind this far. See
  // fatal_handler.hpp for what is left over and why it needs a backstop.
  nsclient::install_fatal_handlers();
  try {
    mainClient.reset(new NSClient());
    cli_parser parser(mainClient);
    const int exit = parser.parse(argc, argv);
    return exit;
  } catch (const std::exception &e) {
    std::cerr << "Exception raised: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Unknown exception raised" << std::endl;
    return 1;
  }
}

//////////////////////////////////////////////////////////////////////////
// Service functions

struct nscp_settings_provider : public settings_manager::provider_interface {
  nsclient::logging::logger_instance log_instance_;
  nsclient::core::path_instance path_;
  nscp_settings_provider(nsclient::core::path_instance path_, nsclient::logging::logger_instance log_instance) : log_instance_(log_instance), path_(path_) {}
  virtual ~nscp_settings_provider() {}

  virtual std::string expand_path(std::string file) { return path_->expand_path(file); }
  nsclient::logging::logger_instance get_logger() const { return log_instance_; }
  void apply_path_overrides(std::map<std::string, std::string> overrides) override { path_->set_overrides(std::move(overrides)); }
  void validate_path_overrides() override { path_->validate_overrides(); }

  void apply_layout(const std::string &mode) override {
    const nscp::paths::layout selected = nscp::paths::parse_layout(mode);
    path_->set_layout(selected);
    if (selected != nscp::paths::layout::legacy) {
      LOG_DEBUG_CORE(std::string("Using the ") + nscp::paths::layout_name(selected) + " layout: shared-path is " + path_->expand_path("${shared-path}"));
    }
  }

  // Create the shared folder and lock it down before anything writes into it.
  // Modern layout only; what it refuses, leaves alone and complains about is
  // shared_folder::prepare().
  void prepare_shared_folder() override {
#ifdef WIN32
    nsclient::core::shared_folder::prepare(*path_, log_instance_);
#endif
  }

  // Export the Windows ROOT certificate store to the file ${ca-path} points at,
  // so every SSL setup site has a sensible default for `ca`.
  //
  // This has to run before the settings store is opened, not merely during
  // startup: a boot.ini settings source of https://... is downloaded while that
  // store is opened, and [tls] now verifies the peer by default against this
  // very bundle. Exporting afterwards meant the file was missing on the first
  // boot of a fresh install - load_verify_file() then throws, the settings
  // download fails, and the agent comes up with no configuration at all. It
  // "worked" from the second boot onwards, because by then the previous boot
  // had written the file.
  //
  // Idempotent: called from settings boot and again from load_configuration_2
  // (which still covers any startup path that does not boot the settings
  // subsystem), but the store is only enumerated once per process.
  void prepare_trust_store() override {
#ifdef WIN32
    if (trust_store_ready_) return;
    trust_store_ready_ = true;
    const std::string bundle_path = path_->expand_path("${ca-path}");
    // Refreshing this is the service's job. On the modern layout the folder is
    // writable only by SYSTEM and Administrators, so an ordinary user running
    // a one-shot command cannot rewrite it - and does not need to, because the
    // service already wrote a usable bundle. Complaining on every such command
    // would be noise about something that is working as designed; complaining
    // when there is no bundle at all is not, because the next TLS call fails.
    const auto report = [this, &bundle_path](const std::string &message) {
      boost::system::error_code ignored;
      if (boost::filesystem::exists(bundle_path, ignored)) {
        LOG_DEBUG_CORE(message + " (keeping the existing bundle)");
      } else {
        LOG_WARN_CORE(message);
      }
    };
    try {
      boost::filesystem::create_directories(boost::filesystem::path(bundle_path).parent_path());
      std::list<std::string> ca_errors;
      const unsigned int n = nsclient::windows_ca::export_root_store(bundle_path, ca_errors);
      for (const std::string &e : ca_errors) report("windows-ca: " + e);
      if (n > 0) LOG_DEBUG_CORE("Exported " + std::to_string(n) + " Windows ROOT certificates to " + bundle_path);
    } catch (const std::exception &e) {
      report(std::string("Failed to export Windows ROOT store: ") + e.what());
    }
#endif
  }

#ifdef WIN32
 private:
  // Only the Windows body has anything to remember; declaring it unconditionally
  // leaves an unused private field everywhere else (-Wunused-private-field).
  bool trust_store_ready_ = false;
#endif
};

nscp_settings_provider *provider_ = NULL;

NSClientT::NSClientT()
    : service_name_(DEFAULT_SERVICE_NAME),
      log_instance_(new nsclient::logging::impl::nsclient_logger()),
      path_(new nsclient::core::path_manager(log_instance_)),
      plugins_(new nsclient::core::plugin_manager(path_, log_instance_)),
      storage_manager_(new nsclient::core::storage_manager(path_, log_instance_)),
      tags_(new nsclient::core::tag_repository()),
      facts_(new nsclient::core::fact_repository()),
      failures_(new nsclient::core::failed_check_store()),
      snapshots_(new nsclient::core::metrics_snapshot_store()),
      diagnostics_(new nsclient::core::diagnostics_snapshot_store()) {
  plugins_->set_fact_repository(facts_);
  plugins_->set_failed_check_store(failures_);
  plugins_->set_metrics_snapshot_store(snapshots_);
  plugins_->set_diagnostics_snapshot_store(diagnostics_);
  provider_ = new nscp_settings_provider(path_, log_instance_);
  log_instance_->startup();
}

NSClientT::~NSClientT() {
  try {
    // The settings core references the provider; take it down first so no
    // proxy holds a pointer into a deleted provider. Idempotent after the
    // shutdown path has already run it.
    settings_manager::destroy_settings();
    delete provider_;
    log_instance_->destroy();
  } catch (...) {
    std::cerr << "UNknown exception raised: When destroying logger" << std::endl;
  }
}

namespace sh = nscapi::settings_helper;

namespace {
// What a settings reader had to say about values it could not use.
void log_setting_errors(const nsclient::logging::logger_instance &log, const std::vector<std::string> &errors) {
  for (const std::string &error : errors) log->error("core", __FILE__, __LINE__, error);
}
}  // namespace

/**
 * Initialize the program
 * @param boot true if we shall boot all plugins
 * @param attachIfPossible is true we will attach to a running instance.
 * @return success
 */
bool NSClientT::load_configuration_1() {
  // TODO: These are split temporarily to allow overriding log-path
#ifdef WIN32
  SetErrorMode(SEM_FAILCRITICALERRORS);
#endif

  LOG_DEBUG_CORE(utf8::cvt<std::string>(SERVICE_NAME) + " Loading settings and logger...");

  // Install CLI --path-override entries as the highest-precedence layer BEFORE
  // init_settings(). This has to happen first because init_settings() opens
  // boot.ini, whose location is itself the resolvable ${boot-conf} token now -
  // applying the overrides afterwards would be too late to relocate boot.ini.
  // The dedicated CLI layer (set_cli_overrides) wins over boot.ini's own
  // [paths], so precedence stays CLI > boot.ini > defaults regardless of order.
  if (!cli_path_overrides_.empty()) {
    LOG_DEBUG_CORE("Applying " + std::to_string(cli_path_overrides_.size()) + " path override(s) from command line");
    path_->set_cli_overrides(cli_path_overrides_);
  }

  if (!settings_manager::init_settings(provider_, context_)) {
    return false;
  }
  // init_settings() judges them itself, at the point where the layout and
  // [paths] are known but nothing has yet acted on a path - see
  // validate_path_overrides(). Repeated here only because a provider is free
  // not to implement that hook; drop_unusable_overrides is idempotent, so an
  // override that already passed simply passes again.
  path_->validate_overrides();
  return true;
}

bool NSClientT::load_configuration_2(const bool override_log) {
  log_instance_->configure();

  LOG_DEBUG_CORE(utf8::cvt<std::string>(SERVICE_NAME) + " booting...");
  LOG_DEBUG_CORE("Booted settings subsystem...");

  bool crash_archive = false;
#ifdef WIN32
  bool crash_restart = false;
#endif
  bool use_credentials = false;
  std::string crash_folder, log_level;
  try {
    sh::settings_registry settings(settings_manager::get_proxy());

    // clang-format off
    settings.add_path()
      (MAIN_MODULES_SECTION, "MODULES", "A list of modules.")
      ("/settings", "Settings", "Core configuration.")
    ;

    settings.add_path_to_settings()
      ("log", "LOG SETTINGS", "Section for configuring the log handling.")
      ("crash", "CRASH HANDLER", "Section for configuring the crash handler.")
      ("default", "Default values", "Default values used in other config sections.")
    ;
    // clang-format on

    settings.add_key_to_path("/settings")
        .add_bool("use credential manager", sh::bool_key(&use_credentials, false), "use credential manager",
                  "Store sensitive keys in use credential manager instead of ini file");

    settings.add_key_to_settings("log").add_string("level", sh::string_key(&log_level, "info"), "LOG LEVEL",
                                                   "Log level to use. Available levels are error,warning,info,debug,trace");

    settings.add_key_to_settings("crash")
        .add_bool("archive", sh::bool_key(&crash_archive, true), "ARCHIVE CRASHREPORTS", "Archive crash reports in the archive folder")
        .add_string("archive folder", sh::path_key(&crash_folder, CRASH_ARCHIVE_FOLDER, "${" CRASH_ARCHIVE_FOLDER_KEY "}"), "CRASH ARCHIVE LOCATION",
                    "The folder to archive crash dumps in. A relative name is taken relative to the crash folder; an absolute path is used as given.");

    settings.register_all();
    // Every key under /modules names a module, so settings validation accepts
    // any key there.
    settings_manager::get_core()->register_user_keys(0xffff, MAIN_MODULES_SECTION);
    settings.notify();
    if (use_credentials) {
      settings_manager::get_settings()->enable_credentials();
    }
  } catch (settings::settings_exception &e) {
    LOG_ERROR_CORE_STD("Could not find settings: " + utf8::utf8_from_native(e.what()));
  }
  if (!override_log) {
    log_instance_->set_log_level(log_level);
  }

  // Repoint the last-resort log - and with it the report the terminate
  // handler writes on the way down (see fatal_handler.hpp) - from the working
  // directory, which for a service is wherever the SCM started it, to the
  // directory the ordinary log already lives in. Anyone told to "send the
  // log" then sends this too.
  //
  // set_fatal_file() proves it can write there before adopting the path and
  // otherwise falls back to the temp folder, so say where the report will
  // actually land: this is the channel that explains a crash, and it silently
  // going nowhere is the one failure nobody would ever notice.
  //
  // Only for the service, which is what override_log distinguishes: the same
  // load path runs for `nscp settings`, `nscp test` and every other command
  // line invocation, and those are typically run with sudo. Letting one of
  // them touch ${log-path} - probe it, and then create it the first time it
  // has a report to write - is how `sudo nscp settings --generate` before the
  // first service start leaves a root-owned log folder that the unprivileged
  // nsclient user the systemd unit runs as can never write to. A command that
  // is not the service keeps the default file and the temp fallback.
  if (!override_log) {
    try {
      const std::string wanted = path_->expand_path("${log-path}/nsclient.fatal");
      const std::string actual = nsclient::logging::logger_helper::set_fatal_file(wanted);
      if (actual == wanted) {
        LOG_DEBUG_CORE("Crash reports will be written to: " + actual);
      } else {
        LOG_ERROR_CORE("Cannot write crash reports to " + wanted + ", using " + actual + " instead");
      }
    } catch (const std::exception &e) {
      LOG_ERROR_CORE_STD("Could not place nsclient.fatal in the log folder, keeping the working directory: " + utf8::utf8_from_native(e.what()));
    }
  }

#ifdef WIN32
  ExceptionManager::instance()->setup_app(APPLICATION_NAME, STRPRODUCTVER, STRPRODUCTDATE);

  if (crash_restart) {
    LOG_DEBUG_CORE("On crash: restart service");
    ExceptionManager::instance()->setup_restart_flag();
  }

  bool crashHandling = false;
  if (crash_archive) {
    ExceptionManager::instance()->setup_path(crash_folder);
    LOG_DEBUG_CORE("Archiving crash dumps in: " + crash_folder);
    crashHandling = true;
  }
  if (!crashHandling) {
    LOG_ERROR_CORE("No crash handling configured");
  } else {
    ExceptionManager::StartMonitoring();
  }
#endif

#ifdef WIN32
  try {
    com_helper_.initialize();
  } catch (com_helper::com_exception &e) {
    LOG_ERROR_CORE_STD("COM exception: " + e.reason());
    return false;
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception initiating COM...");
    return false;
  }

  // Export the Windows ROOT certificate store to the file pointed at by the
  // ${ca-path} setting so SSL setup sites have a sensible default for `ca`.
  // Normally already done from the settings boot (a remote settings source is
  // verified against this bundle, so it cannot wait until here); this call
  // covers any startup path that skipped that, and is a no-op otherwise.
  if (provider_ != NULL) provider_->prepare_trust_store();
#endif

  boost::filesystem::path pluginPath = path_->expand_path("${module-path}");
  if (!boost::filesystem::is_directory(pluginPath)) {
    const auto tmpPluginPath = path_->expand_path("${exe-path}/modules");
    if (boost::filesystem::is_directory(tmpPluginPath)) {
      LOG_WARN_CORE("Modules folder " + pluginPath.string() + " not found using " + tmpPluginPath + " instead.");
      pluginPath = tmpPluginPath;
    } else {
      LOG_ERROR_CORE("Failed to find modules folder: " + pluginPath.string());
      return false;
    }
  }
  plugins_->set_path(pluginPath);

  return true;
}
bool NSClientT::boot_load_active_plugins() {
  try {
    // Permissions sit on the request-dispatch path inside the plugin
    // manager, so they need to be loaded BEFORE any plugin can run a
    // command. Load order is settings -> permissions -> plugins.
    plugins_->load_permissions();
    plugins_->load_active_plugins();
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Exception loading modules: " + utf8::utf8_from_native(e.what()));
    return false;
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception when loading plugins");
    return false;
  }
  return true;
}

void NSClientT::boot_load_all_plugin_files() {
  try {
    plugins_->load_all_plugins();
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Exception loading modules: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception when loading plugins");
  }
}

bool NSClientT::boot_load_single_plugin(const std::string &plugin) {
  try {
    return plugins_->load_single_plugin(std::move(plugin));
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Exception loading modules: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception when loading plugins");
  }
  return false;
}

bool NSClientT::boot_start_plugins(bool boot) {
  storage_manager_->load();
  try {
    plugins_->start_plugins(boot ? NSCAPI::normalStart : NSCAPI::dontStart);
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception loading plugins");
    return false;
  }
  if (boot) {
    settings_manager::get_core()->register_key(0xffff, "/settings/core", "settings maintenance interval", "string", "Maintenance interval",
                                               "How often settings shall reload config if it has changed", "5m", true, false);
    std::vector<std::string> interval_errors;
    nsclient::core::core_settings::schedule_or_fallback(
        [this](const std::string &interval) { scheduler_.add_task(task_scheduler::schedule_metadata::SETTINGS, interval); }, "'settings maintenance interval'",
        settings_manager::get_settings()->get_string("/settings/core", "settings maintenance interval", "5m"), "5m", interval_errors);
    settings_manager::get_core()->register_key(0xffff, "/settings/core", "metrics interval", "string", "Metrics interval",
                                               "How often to fetch metrics from modules", "10s", true, false);
    nsclient::core::core_settings::schedule_or_fallback(
        [this](const std::string &interval) { scheduler_.add_task(task_scheduler::schedule_metadata::METRICS, interval); }, "'metrics interval'",
        settings_manager::get_settings()->get_string("/settings/core", "metrics interval", "10s"), "10s", interval_errors);
    log_setting_errors(log_instance_, interval_errors);
    settings_manager::get_core()->register_key(0xffff, "/settings/core", "diagnostics interval", "string", "Diagnostics interval",
                                               "How often to collect diagnostics (the tables kept with a failed check and served on "
                                               "/api/v2/diagnostics) from the modules that produce them. Empty, the default, collects them at the "
                                               "end of every metrics round, on the same thread. Only used while [/settings/core/failed checks] "
                                               "capture diagnostics is on. Read when the service starts.",
                                               "", true, false);
    const std::string di = settings_manager::get_settings()->get_string("/settings/core", "diagnostics interval", "");
    diagnostics_with_metrics_ = di.empty();
    if (!diagnostics_with_metrics_) {
      try {
        scheduler_.add_task(task_scheduler::schedule_metadata::DIAGNOSTICS, di);
      } catch (const std::exception &e) {
        LOG_ERROR_CORE_STD("Invalid 'diagnostics interval' value '" + di + "', collecting with the metrics round instead: " + utf8::utf8_from_native(e.what()));
        diagnostics_with_metrics_ = true;
      }
    }
    settings_manager::get_core()->register_key(0xffff, "/settings/core", "settings maintenance threads", "int", "Maintenance thread count",
                                               "How many threads will run in the background to maintain the various core helper tasks.", "1", true, false);
    int count = str::stox<int>(settings_manager::get_settings()->get_string("/settings/core", "settings maintenance threads", "1"));
    scheduler_.set_threads(count);
    boot_facts();
    boot_failed_checks();
    scheduler_.start();
  }
  try {
    if (boot) {
      plugins_->post_start_plugins();
    }
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception starting plugins");
    return false;
  }
  if (boot) {
    // One round before the fleet loop sends its first state report, so a fresh
    // host reports the inventory it has rather than an empty document it will
    // correct a minute later.
    process_facts("startup");
    boot_fleet_sync();
    report_settings_problems();
  }
  LOG_DEBUG_CORE(utf8::cvt<std::string>(APPLICATION_NAME " - " CURRENT_SERVICE_VERSION " Started!"));
  return true;
}

void NSClientT::report_settings_problems() {
  try {
    const settings::validation_result problems = settings_manager::get_core()->validate();
    const std::size_t warnings = settings::count_warnings(problems);
    if (warnings > 0) {
      LOG_WARN_CORE_STD("Configuration: " + str::xtos(warnings) +
                        (warnings == 1 ? " entry is ignored because nothing reads it" : " entries are ignored because nothing reads them") +
                        ". Run 'nscp settings --validate', or see Configuration problems in the web UI, for details.");
    }
    for (const settings::validation_finding &f : problems) {
      const std::string line = "Configuration: " + f.message + " (" + f.source + ")";
      if (f.level == settings::validation_finding::severity::warning) {
        LOG_WARN_CORE_STD(line);
      } else {
        LOG_DEBUG_CORE_STD(line);
      }
    }
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Failed to validate the configuration: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Failed to validate the configuration: UNKNOWN");
  }
}

// Register the core's facts settings and, when a module can produce facts at
// all, the round that refreshes them.
//
// Enablement itself is not here: which fact sets a module produces is that
// module's own configuration, next to everything else it is configured with,
// so the core never reads an enable list. This section owns only the two keys
// that pace the round, and the task is not registered at all when no loaded
// module produces facts.
void NSClientT::boot_facts() {
  try {
    const std::string path = "/settings/facts";
    settings_manager::get_core()->register_path(0xffff, path, "Host inventory (facts)",
                                                "How often the host inventory is refreshed and how large it may get. Which fact sets are collected is "
                                                "configured in the module that produces them; nothing is collected until one is enabled there.",
                                                true, false);
    settings_manager::get_core()->register_key(0xffff, path, "interval", "string", "Refresh interval",
                                               "How often the core asks every module to refresh the fact sets it is configured to produce.", "1h", true, false);
    settings_manager::get_core()->register_key(0xffff, path, "max size", "int", "Maximum document size",
                                               "Size budget for the whole facts document, counted on its encoded form. A fact set that would take the "
                                               "document past it is rejected, and the previous value of that set is kept.",
                                               str::xtos(nsclient::core::fact_repository::default_max_size), true, false);
    settings_manager::get_core()->register_key(0xffff, path, "agent", "bool", "AGENT FACTS",
                                               "Collect the `agent` fact set: the NSClient++ version, the modules it has loaded and whether it is enrolled "
                                               "with a fleet server (yes or no - never which server or with which identity). The one set the core produces "
                                               "itself; it is re-read at start and on every settings reload, which is when any of it can change.",
                                               "false", false, false);

    read_facts_max_size();

    if (!plugins_->has_facts_fetchers()) {
      LOG_DEBUG_CORE("No loaded module produces facts, inventory will not be collected");
      return;
    }
    std::vector<std::string> interval_errors;
    nsclient::core::core_settings::schedule_or_fallback(
        [this](const std::string &interval) { scheduler_.add_task(task_scheduler::schedule_metadata::FACTS, interval); }, "facts 'interval'",
        settings_manager::get_settings()->get_string(path, "interval", "1h"), "1h", interval_errors);
    log_setting_errors(log_instance_, interval_errors);
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Failed to configure facts: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Failed to configure facts");
  }
}

// The failed-check log's section. Every key is re-read on a settings reload
// (read_failed_check_settings), so an operator can turn argument recording on
// to debug a failing check and off again without restarting the service.
void NSClientT::boot_failed_checks() {
  try {
    const std::string path = "/settings/core/failed checks";
    settings_manager::get_core()->register_path(0xffff, path, "Failed-check log",
                                                "The last few check results that did not come back OK, whatever asked for them (NRPE, REST, the scheduler, "
                                                "a check that runs other checks), kept in memory and served on /api/v2/failures and the web UI. Every key "
                                                "here is re-read on a settings reload.",
                                                true, false);
    settings_manager::get_core()->register_key(0xffff, path, "enabled", "bool", "Record failed checks",
                                               "Keep the last `max entries` non-OK check results. Turning it off empties the log.", "true", true, false);
    settings_manager::get_core()->register_key(0xffff, path, "max entries", "int", "How many to keep",
                                               "The newest results are kept and the oldest dropped once the log is full. Each entry holds one check's "
                                               "output and performance data, so a few hundred is cheap; the cap is " +
                                                   str::xtos(nsclient::core::failed_check_store::max_max_entries) + ".",
                                               str::xtos(nsclient::core::failed_check_store::default_max_entries), true, false);
    settings_manager::get_core()->register_key(
        0xffff, path, "record arguments", "string", "Which of the check's arguments to record",
        "Store the options each failed check was called with, so the log shows what was asked and not only what came back. none keeps "
        "nothing; safe (the default) keeps only the options that describe the check - the thresholds, the filter and the syntax options, and "
        "what was checked (time, type, drive, service, process, file, path, counter, ...) - and drops everything else, including anything "
        "that names a host, a URL, a command or a query; all keeps every option. An argument can carry a credential (a database password, a "
        "token for a forwarded check): under all, any argument whose name mentions a password or a token is stored as `***`, but a secret "
        "passed under another name is not. true and false are read as all and none.",
        nsclient::core::failed_check_store::format_argument_mode(nsclient::core::failed_check_store::argument_mode::safe), true, false);
    settings_manager::get_core()->register_key(0xffff, path, "statuses", "string", "Which results count as failed",
                                               "A comma-separated list of warning, critical and unknown. Drop warning to log only the results that need "
                                               "someone to act. OK is never recorded.",
                                               nsclient::core::failed_check_store::format_statuses(nsclient::core::failed_check_store::default_statuses), true,
                                               false);
    settings_manager::get_core()->register_key(
        0xffff, path, "capture metrics", "bool", "Record the system's metrics with each failure",
        "Keep the agent's latest metrics snapshot (what /api/v2/metrics serves: CPU, memory, disks, network, whatever the loaded modules publish) "
        "with every failed check, so the log shows what the system looked like when it failed. The snapshot is at most one `metrics interval` "
        "older than the failure, and failures within the same interval share one. Off by default: a snapshot can hold host details (names, "
        "versions, addresses) that the check output alone does not, and it is reported to an enrolled fleet server along with the failure.",
        "false", true, false);
    settings_manager::get_core()->register_key(0xffff, path, "metrics context budget", "int", "Memory for metrics snapshots (bytes)",
                                               "The most the log spends on snapshots, counting each distinct snapshot once however many failures refer "
                                               "to it. Once it is used up the oldest snapshots are released and their failures stay in the log, "
                                               "marked as evicted. Clamped to " +
                                                   str::xtos(nsclient::core::failed_check_store::min_context_budget) + "-" +
                                                   str::xtos(nsclient::core::failed_check_store::max_context_budget) + ".",
                                               str::xtos(nsclient::core::failed_check_store::default_context_budget), true, false);
    settings_manager::get_core()->register_key(0xffff, path, "max context size", "int", "Largest snapshot kept (bytes)",
                                               "A snapshot larger than this is not kept, and the failure is marked as too large. Never more than the "
                                               "budget.",
                                               str::xtos(nsclient::core::failed_check_store::default_max_context_size), true, false);
    settings_manager::get_core()->register_key(
        0xffff, path, "prune metrics", "string", "Trim the metrics kept with a failure",
        "A comma-separated list of rules applied to a snapshot before it is kept with a failure, and so before it is reported to a fleet "
        "server: zeros drops every numeric value that is 0 (except the pressure metrics - system.pressure, disk queue lengths, memory pages/sec - "
        "and the hour and day change in a volume's used space, where 0 is the reading that matters), empty every empty string, and duplicates every metric that only restates others "
        "(memory avail and %, a disk's free and percentages, CPU idle - used and total are kept) and every instance (a disk, an adapter, a "
        "core) whose values are all identical to an earlier one in the same section - `_Total` and the only disk, a row of idle adapters. "
        "The first instance is kept and the ones it stands for are listed with the context, so nothing is silently lost. Empty keeps the "
        "snapshot whole; /api/v2/metrics is never pruned.",
        "", true, false);
    settings_manager::get_core()->register_key(
        0xffff, path, "prune pattern", "string", "Leave out metrics matching",
        "A comma-separated list of text: every metric whose key (as /api/v2/metrics shows it) contains one of them is left out of the "
        "snapshot kept with a failure, ignoring the case of A-Z. Use it for the instances that are noise on your hosts, for example "
        "`HarddiskVolume8,Hyper-V Virtual,VirtualBox`. Applied before `prune metrics`, so an excluded instance is never the one a "
        "duplicate is collapsed into.",
        "", true, false);
    settings_manager::get_core()->register_key(
        0xffff, path, "capture diagnostics", "bool", "Collect diagnostics and record them with each failure",
        "Collect diagnostics - small tables of what is going on right now, such as the processes using the most CPU and memory and the "
        "last failed checks - every `diagnostics interval` (by default with the metrics round), keep the latest with every failed check, "
        "and serve it on /api/v2/diagnostics. Which tables a module produces is switched on in that module's own diagnostics section "
        "(for example [/settings/system/windows/diagnostics] processes); with none on, a failure carries only the last failed checks. "
        "Off by default: the tables hold process names and other host details, and they are reported to an enrolled fleet server along with "
        "the failure. Snapshots share `metrics context budget` and `max context size` with the metrics snapshots, and `prune pattern` "
        "leaves out a table whose name matches.",
        "false", true, false);
    settings_manager::get_core()->register_key(0xffff, path, "diagnostics rows", "int", "Rows per diagnostics table",
                                               "The most rows kept per table (the top processes, the last failed checks). A table cut by it is "
                                               "marked as truncated. 1-" +
                                                   str::xtos(nscapi::diagnostics::max_rows) + ".",
                                               str::xtos(nsclient::core::diagnostics_round::default_max_rows), true, false);
    read_failed_check_settings();
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Failed to configure the failed-check log: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Failed to configure the failed-check log");
  }
}

void NSClientT::read_failed_check_settings() {
  // What each key means and what an unusable value falls back to:
  // core_settings::read_failed_checks().
  std::vector<std::string> errors;
  try {
    const nsclient::core::failed_check_store::options options = nsclient::core::core_settings::read_failed_checks(
        [](const std::string &key, const std::string &fallback) {
          return settings_manager::get_settings()->get_string(nsclient::core::core_settings::failed_checks_section, key, fallback);
        },
        failures_->get_options(), errors);
    failures_->configure(options);
  } catch (const std::exception &e) {
    errors.push_back("Failed to read the failed-check log settings: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    errors.push_back("Failed to read the failed-check log settings");
  }
  log_setting_errors(log_instance_, errors);
}

// [/settings/facts] max size, applied to the repository. Read at boot, on a
// settings-only reload, and on a full reload before its facts round, so a cap
// that was raised or lowered applies without a restart.
void NSClientT::read_facts_max_size() {
  std::vector<std::string> errors;
  nsclient::core::core_settings::apply_facts_max_size(
      *facts_, settings_manager::get_settings()->get_string("/settings/facts", "max size", str::xtos(nsclient::core::fact_repository::default_max_size)),
      errors);
  log_setting_errors(log_instance_, errors);
}

// Start the fleet configuration sync loop - but only when this host has been
// enrolled, i.e. the enrollment manifest written by `nscp enroll` exists.
// Un-enrolled hosts pay nothing: no thread, no traffic, just a debug line.
void NSClientT::boot_fleet_sync() {
#ifdef HAVE_ONBOARDING
  try {
    const std::string path = nsclient::core::fleet_settings::section;
    settings_manager::get_core()->register_path(0xffff, path, "Fleet configuration sync",
                                                "Settings for keeping this host in sync with an NSClient fleet server (active once the host is "
                                                "enrolled with `nscp enroll`).",
                                                true, false);
    // The keys, their descriptions and what is made of their values:
    // fleet_settings::read().
    std::vector<std::string> setting_errors;
    fleet_config config = nsclient::core::fleet_settings::read(
        [&path](const char *key, const char *type, const char *title, const char *description, const char *fallback) {
          settings_manager::get_core()->register_key(0xffff, path, key, type, title, description, fallback, true, false);
          return settings_manager::get_settings()->get_string(path, key, fallback);
        },
        *path_, setting_errors);
    log_setting_errors(log_instance_, setting_errors);
    config.nscp_version = CURRENT_SERVICE_VERSION;
    config.local_config_probe = [] { return settings_manager::has_local_configuration(); };
    // Same boot.ini opt-in that allows a plaintext settings source: whether
    // this agent may take its configuration over an unauthenticated channel is
    // one question, and the fleet channel is the same channel by another name.
    config.allow_plaintext = settings_manager::get_core()->get_allow_plaintext();

    std::string manifest_detail;
    const fleet_sync::manifest_status manifest = fleet_sync::check_manifest(config.state_file, manifest_detail);
    if (manifest == fleet_sync::manifest_status::missing) {
      LOG_DEBUG_CORE_STD("No fleet enrollment manifest (" + config.state_file + "): fleet sync not started");
      return;
    }
    if (manifest == fleet_sync::manifest_status::unreadable) {
      // This host IS enrolled, so staying quiet would leave an agent that looks
      // healthy and never reports to the fleet. Name the fix in the same line.
      LOG_ERROR_CORE_STD("Fleet enrollment manifest cannot be read (" + config.state_file + "): " + manifest_detail +
                         ". Fleet sync not started - grant the service account read access to it (chown it to the owner of " +
                         path_->expand_path("${data-path}") + ") and restart.");
      return;
    }
    const std::shared_ptr<fleet_sync> sync =
        std::make_shared<fleet_sync>(log_instance_, config, tags_, facts_, failures_, [this] { this->reload("delayed,service"); });
    {
      boost::mutex::scoped_lock lock(fleet_sync_mutex_);
      fleet_sync_ = sync;
    }
    log_instance_->info("fleet", __FILE__, __LINE__, "Fleet configuration sync started (manifest: " + config.state_file + ")");
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Failed to start fleet sync: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Failed to start fleet sync: UNKNOWN");
  }
#endif
}

#ifdef HAVE_ONBOARDING
std::shared_ptr<fleet_sync> NSClientT::get_fleet_sync() const {
  boost::mutex::scoped_lock lock(fleet_sync_mutex_);
  return fleet_sync_;
}
#endif

void NSClientT::stop_fleet_sync() {
#ifdef HAVE_ONBOARDING
  std::shared_ptr<fleet_sync> sync;
  {
    boost::mutex::scoped_lock lock(fleet_sync_mutex_);
    sync.swap(fleet_sync_);
  }
  if (sync) {
    LOG_DEBUG_CORE("Stopping fleet sync");
    sync->stop();
  }
#endif
}

bool NSClientT::stop_nsclient() {
  // Stop the fleet sync first: it can request (delayed) reloads, which make
  // no sense once shutdown has begun.
  stop_fleet_sync();
  scheduler_.stop();
  LOG_DEBUG_CORE("Attempting to stop all plugins");
  try {
    LOG_DEBUG_CORE("Preparing shutdown of all plugins");
    plugins_->prepare_shutdown_plugins();
  } catch (nsclient::core::plugin_exception &e) {
    LOG_ERROR_CORE_STD("Exception raised when preparing shutdown of plugins: " + e.reason() + " in module: " + e.file());
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception raised when preparing shutdown of plugins");
  }
  try {
    LOG_DEBUG_CORE("Stopping all plugins");
    unloadPlugins();
  } catch (nsclient::core::plugin_exception &e) {
    LOG_ERROR_CORE_STD("Exception raised when unloading non msg plguins: " + e.reason() + " in module: " + e.file());
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception raised when unloading non msg plugins");
  }
  storage_manager_->save();
#ifdef WIN32
  LOG_DEBUG_CORE("Stopping: COM helper");
  try {
    com_helper_.unInitialize();
  } catch (com_helper::com_exception &e) {
    LOG_ERROR_CORE_STD("COM exception: " + e.reason());
  } catch (...) {
    LOG_ERROR_CORE("Unknown exception uninitiating COM...");
  }
#endif
  LOG_DEBUG_CORE("Stopping: Settings instance");
  settings_manager::destroy_settings();
  try {
    log_instance_->shutdown();
    google::protobuf::ShutdownProtobufLibrary();
  } catch (...) {
    LOG_ERROR_CORE("UNknown exception raised: When stopping");
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////
// Member functions

// The log subscriptions are cleared by stop_plugins itself, under its
// lifecycle lock, so an unload on another thread cannot interleave with it.
void NSClientT::unloadPlugins() { plugins_->stop_plugins(); }
void NSClientT::reloadPlugins() {
  // Re-read the included configuration before working out which modules should
  // be running. An include is served from the child instance built when the
  // configuration was last loaded, so a module enabled in one since then -
  // fleet.ini, rewritten by the fleet sync, being the case that matters - is
  // invisible to find_all_active_plugins() and never gets loaded. That is how a
  // fleet bundle enabling a module ended up doing nothing at all: the file on
  // disk said the module was enabled and the host reported itself in sync,
  // while the module only really appeared at the next service restart.
  //
  // Only the children are refreshed, and deliberately so. Clearing the whole
  // store would also throw away configuration that was set in memory and never
  // saved - which is precisely how `nscp unit` and `nscp client` work: they
  // configure the agent in memory and then reload to apply it. Dropping that
  // leaves them with no modules at all.
  for (const settings::instance_ptr &child : settings_manager::get_settings()->get_children()) {
    if (!child) continue;
    // A missing include is not an error (the backend just comes back empty),
    // but a file that cannot be *read* - permissions, transient I/O - throws,
    // and by then this child's caches are already emptied. Catch it here so
    // one broken include costs only its own content: the remaining includes
    // still refresh and the reload still runs. Letting it escape would abort
    // the whole reload before the re-scan, when the store is at its most
    // stale.
    try {
      child->clear_cache();
    } catch (const std::exception &e) {
      LOG_ERROR_CORE_STD("Failed to re-read included configuration " + child->get_context() + ": " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      LOG_ERROR_CORE_STD("Failed to re-read included configuration " + child->get_context());
    }
  }
  plugins_->start_plugins(NSCAPI::reloadStart);
  // Loads whatever is enabled and not already loaded; modules that are
  // running are recognised as duplicates and left alone.
  boot_load_active_plugins();
  plugins_->start_plugins(NSCAPI::normalStart);
  // Now that every module the reload enabled is loaded: start the ones it
  // loaded, and start again the ones it reloaded. A module only started by
  // this does its startup work - a Lua on_start querying a module enabled in
  // the same reload, Scheduler's run-on-startup schedules - against a
  // complete agent. Modules loaded here were never started at all before.
  plugins_->post_start_plugins();
  // TODO: a module *disabled* since the last load is still left running; that
  // needs unloading a live plugin, which is a different problem from this one.
  settings_manager::get_core()->set_reload(false);
  report_settings_problems();
  // The reloaded configuration may have enabled or disabled fact sets, and a
  // set that is no longer enabled has to leave the document now rather than at
  // the next hourly round.
  process_facts("reload", true);
  read_failed_check_settings();
}

bool NSClientT::do_reload(const std::string module) {
  if (module == "settings") {
    try {
      settings_manager::get_settings()->clear_cache();
      // Re-read permission policies from the refreshed settings store so
      // operators can adjust rules without a full service reload. Plugin
      // configs are reloaded via the per-plugin loadModuleEx path; this
      // catches the core-side state.
      plugins_->load_permissions();
      // And [/settings/facts] max size, which a settings-only reload has to
      // apply as well as a full one: a lowered cap drops the sets that no
      // longer fit now, a raised one lets the next round keep them. Inside a
      // round, so the drop is published as one change.
      // The `agent` set's switch lives in the same section and is documented
      // as re-read on every settings reload, so it is collected in the same
      // round.
      {
        const nsclient::core::fact_repository::scoped_round round(*facts_);
        read_facts_max_size();
        collect_agent_facts();
      }
      read_failed_check_settings();
      report_settings_problems();
      return true;
    } catch (const std::exception &e) {
      LOG_ERROR_CORE_STD("Exception raised when reloading: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      LOG_ERROR_CORE("Exception raised when reloading: UNKNOWN");
    }
  } else if (module == "service") {
    try {
      LOG_DEBUG_CORE_STD("Reloading all modules.");
      reloadPlugins();
      return true;
    } catch (const std::exception &e) {
      LOG_ERROR_CORE_STD("Exception raised when reloading: " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      LOG_ERROR_CORE("Exception raised when reloading: UNKNOWN");
    }
  } else {
    return plugins_->reload_plugin(module);
  }
  return false;
}

NSCAPI::errorReturn NSClientT::reload(const std::string module) {
  try {
    const nsclient::core::reload_request::request request = nsclient::core::reload_request::parse(module);
    // Why a reload asked for from inside a module is deferred or refused:
    // reload_request::decide().
    nsclient::core::reload_request::caller caller;
    caller.inside_any_module = plugins_->is_dispatching_on_this_thread();
    caller.inside_target = plugins_->is_dispatching_on_this_thread(request.task);
    caller.scheduler_running = scheduler_.is_running();
    switch (nsclient::core::reload_request::decide(request, caller)) {
      case nsclient::core::reload_request::action::refuse:
        LOG_ERROR_CORE_STD("Refusing to reload " + request.task +
                           " from inside a call into the module being reloaded while the scheduler is not running: request it from outside the module");
        return NSCAPI::api_return_codes::hasFailed;
      case nsclient::core::reload_request::action::defer:
        if (request.delayed) {
          LOG_TRACE_CORE("Delayed reload");
        } else {
          LOG_DEBUG_CORE_STD("Reload of " + request.task + " requested from inside a call into a module: deferring it to the scheduler");
        }
        scheduler_.add_task(task_scheduler::schedule_metadata::RELOAD, "", request.task);
        return NSCAPI::api_return_codes::isSuccess;
      case nsclient::core::reload_request::action::run_inline:
        break;
    }
    LOG_TRACE_CORE("Instant reload");
    return do_reload(request.task) ? NSCAPI::api_return_codes::isSuccess : NSCAPI::api_return_codes::hasFailed;
  } catch (const std::exception &e) {
    LOG_ERROR_CORE("Reload failed: " + utf8::utf8_from_native(e.what()));
    return NSCAPI::api_return_codes::hasFailed;
  } catch (...) {
    LOG_ERROR_CORE("Reload failed");
    return NSCAPI::api_return_codes::hasFailed;
  }
}

// Service API
NSClient *NSClientT::get_global_instance() { return mainClient.get(); }
void NSClientT::handle_startup(std::string service_name) {
  LOG_DEBUG_CORE("Starting: " + service_name);
  service_name_ = service_name;
#ifdef WIN32
  ExceptionManager::instance()->setup_service_name(service_name);
#endif
  load_configuration_1();
  load_configuration_2();
  boot_load_active_plugins();
  boot_start_plugins(true);
  LOG_DEBUG_CORE("Starting: DONE");
}
void NSClientT::handle_shutdown(std::string service_name) { stop_nsclient(); }

NSClientT::service_controller NSClientT::get_service_control() { return service_controller(service_name_); }

void NSClientT::service_controller::stop() {
#ifdef WIN32
  win_service_control::StopNoWait(utf8::cvt<std::wstring>(get_service_name()));
#endif
}
void NSClientT::service_controller::start() {
#ifdef WIN32
  win_service_control::Start(utf8::cvt<std::wstring>(get_service_name()));
#endif
}
bool NSClientT::service_controller::is_started() {
#ifdef WIN32
  try {
    if (win_service_control::isStarted(utf8::cvt<std::wstring>(get_service_name()))) {
      return true;
    }
  } catch (...) {
    return false;
  }
#endif
  return false;
}

PB::Metrics::MetricsBundle NSClientT::ownMetricsFetcher() {
  using nscapi::metrics::describe;
  using nscapi::metrics::metric;

  PB::Metrics::MetricsBundle bundle;
  bundle.set_key("workers");
  describe(&bundle, "The agent's own scheduler thread pool");
  if (scheduler_.get_scheduler().has_metrics()) {
    // Executed, completed and errored only ever grow while the agent runs, so
    // they are counters: a scraper may take a rate of them, and the reset on
    // restart is the one thing a counter is defined to survive.
    metric(&bundle, "jobs").help("Scheduled jobs the agent has started since it was started").counter(scheduler_.get_scheduler().get_metric_executed());
    metric(&bundle, "submitted")
        .help("Scheduled jobs the agent has completed since it was started")
        .counter(scheduler_.get_scheduler().get_metric_compleated());
    metric(&bundle, "errors").help("Scheduled jobs that failed since the agent was started").counter(scheduler_.get_scheduler().get_metric_errors());
    metric(&bundle, "threads").help("Threads currently in the scheduler pool").gauge(scheduler_.get_scheduler().get_metric_threads());
    metric(&bundle, "refresh_interval").help("How often the agent collects a metrics snapshot").unit("seconds").gauge(scheduler_.get_metrics_interval());
  } else {
    metric(&bundle, "metrics.available").help("Whether the scheduler is collecting metrics at all").gauge(0);
  }
  return bundle;
}
void NSClientT::process_metrics() {
  PB::Metrics::MetricsMessage snapshot = plugins_->process_metrics(ownMetricsFetcher());
  // Kept only while something wants it: the failed-check log records the
  // latest snapshot with each failure when `capture metrics` is on, and
  // otherwise the core holds nothing between rounds.
  if (failures_->captures_metrics()) {
    snapshots_->store(std::move(snapshot), nsclient::core::failed_check_store::now(), failures_->get_options().prune);
  } else {
    snapshots_->reset();
  }
  if (diagnostics_with_metrics_) process_diagnostics();
}

// The diagnostics round. Runs on the maintenance thread, with the metrics
// round unless `diagnostics interval` gives it a task of its own, and only
// while `capture diagnostics` is on. Never because a check failed or a reader
// asked: a failure attaches what the last round left.
void NSClientT::process_diagnostics() {
  try {
    if (!failures_->captures_diagnostics()) {
      diagnostics_->reset();
      return;
    }
    const nsclient::core::failed_check_store::options options = failures_->get_options();
    nsclient::core::diagnostics_round::caps limits;
    limits.max_rows = nsclient::core::diagnostics_round::clamp_rows(options.diagnostics_rows);
    limits.max_size = options.max_context_size;
    limits.prune_patterns = options.prune.patterns;
    const std::int64_t now = nsclient::core::failed_check_store::now();

    // The core's own section leads, so the section cap never cuts it.
    PB::Diagnostics::DiagnosticsMessage collected;
    nsclient::core::diagnostics_round::add_recent_failures(collected, failures_->list(0, limits.max_rows + 1), limits.max_rows, now);
    PB::Diagnostics::DiagnosticsMessage produced = plugins_->collect_diagnostics();
    for (PB::Diagnostics::Section &section : *produced.mutable_sections()) collected.add_sections()->Swap(&section);
    const nsclient::core::diagnostics_snapshot snapshot = nsclient::core::diagnostics_round::assemble(std::move(collected), limits, now);
    if (diagnostics_oversized_.exchange(snapshot.oversized) != snapshot.oversized && snapshot.oversized > 0) {
      LOG_WARN_CORE_STD("Diagnostics: " + str::xtos(snapshot.oversized) +
                        " table(s) left out of the snapshot, which does not fit [/settings/core/failed checks] max context size (" +
                        str::xtos(limits.max_size) + " bytes) even without rows");
    }
    diagnostics_->store(snapshot);
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Failed to collect diagnostics: " + utf8::utf8_from_native(e.what()));
  } catch (...) {
    LOG_ERROR_CORE("Failed to collect diagnostics");
  }
}
void NSClientT::process_facts(const std::string &reason, const bool reread_max_size) {
  // One round, published whole: the fleet sync, polling on its own thread,
  // hashes and uploads the document as the last round left it until this one
  // has stored every set and marked the time.
  const nsclient::core::fact_repository::scoped_round round(*facts_);
  // A reload's new cap is applied inside the same round: the sets it drops
  // and the ones the round stores are one publication, not two uploads.
  if (reread_max_size) read_facts_max_size();
  collect_agent_facts();
  plugins_->process_facts(reason);
}

// The `agent` set. Read from the settings on every round rather than cached at
// boot, so a reload that switches it takes effect on the reload round without
// the core keeping a copy of its own switch.
//
// Nothing in it changes between reloads - the version is compiled in, the
// module list moves only when modules are loaded, and enrolling takes a
// restart - so it needs no round of its own: the startup and reload rounds are
// the ones that can see a difference, and the hourly round, when there is one,
// merely confirms it (an unchanged set costs no revision).
void NSClientT::collect_agent_facts() {
  const unsigned int owner = nsclient::core::fact_repository::core_owner;
  bool enabled = false;
  try {
    enabled = settings::settings_interface::string_to_bool(settings_manager::get_settings()->get_string("/settings/facts", "agent", "false"));
  } catch (const std::exception &e) {
    LOG_ERROR_CORE_STD("Failed to read the facts 'agent' setting: " + utf8::utf8_from_native(e.what()));
    return;
  }
  if (!enabled) {
    // Turned off, or never on: the same omission that drops a module's set.
    facts_->retain_only(owner, {});
    return;
  }
  std::string error;
  const PB::Facts::Object agent = nsclient::core::agent_facts::build(CURRENT_SERVICE_VERSION, plugins_->get_loaded_modules(), is_enrolled());
  if (facts_->set(nsclient::core::agent_facts::set_agent, owner, agent, error) == nsclient::core::fact_repository::set_result::rejected) {
    LOG_ERROR_CORE_STD("The agent fact set was rejected: " + error);
  }
  facts_->retain_only(owner, {nsclient::core::agent_facts::set_agent});
}

// Whether this host is enrolled with a fleet server: the same test the fleet
// sync makes before it starts, that the enrollment manifest exists. Asked of
// the file rather than of the running loop because the first facts round runs
// before the loop is started - on purpose, so the first report carries the
// inventory - and would otherwise always say no.
bool NSClientT::is_enrolled() {
#ifdef HAVE_ONBOARDING
  try {
    const std::string state_file = settings_manager::get_settings()->get_string("/settings/fleet", "state file", DEFAULT_FLEET_STATE_LOCATION);
    return boost::filesystem::exists(path_->expand_path(state_file));
  } catch (const std::exception &) {
    return false;
  }
#else
  return false;
#endif
}

#ifdef _WIN32
void NSClientT::handle_session_change(unsigned long dwSessionId, bool logon) {}
#endif
