// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <config.h>

#include <boost/filesystem/operations.hpp>
#include <boost/unordered_set.hpp>
#include <nscapi/settings/helper.hpp>
#include <settings/settings_core.hpp>

#include "../libs/settings_manager/settings_manager_impl.h"
#include "NSClient++.h"
#include "cli_parser.hpp"
#include "core_api.h"
#include "logger/nsclient_logger.hpp"
#ifdef HAVE_ONBOARDING
#include <net/socket/socket_helpers.hpp>
#include <str/format.hpp>

#include "fleet_sync.hpp"
#endif
#ifdef WIN32
#include "windows_ca_store.hpp"
#endif

#ifdef WIN32
#include <win/acl.hpp>
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

  void apply_layout(const std::string &mode) override {
    const nscp::paths::layout selected = nscp::paths::parse_layout(mode);
    path_->set_layout(selected);
    if (selected != nscp::paths::layout::legacy) {
      LOG_DEBUG_CORE(std::string("Using the ") + nscp::paths::layout_name(selected) + " layout: shared-path is " + path_->expand_path("${shared-path}"));
    }
  }

  // Create the shared folder and lock it down before anything writes into it.
  //
  // Only for the modern layout: the legacy one lives in the install directory,
  // whose permissions are the installer's business. On the modern layout the
  // folder sits under %ProgramData%, which grants Users: Read & Execute by
  // inheritance - so creating it and walking away would publish the
  // configuration (passwords), the fleet private key and the log to every
  // account on the machine.
  void prepare_shared_folder() override {
#ifdef WIN32
    if (path_->get_layout() != nscp::paths::layout::modern) return;
    try {
      const std::string shared = path_->expand_path("${shared-path}");
      if (shared.empty()) return;

      // Look before acting. This runs during the settings bootstrap of *every*
      // process, not just the service: an unelevated `nscp client ...` has no
      // WRITE_DAC, so it used to fail below and announce that a folder which is
      // in fact locked down "may be readable by every user on this machine".
      // Restricting it is the service's job, and the same reasoning as the
      // trust-store export below applies - complaining about something working
      // as designed is noise that trains operators to ignore the message.
      boost::system::error_code ec;
      if (boost::filesystem::is_directory(shared, ec)) {
        std::list<std::string> state_errors;
        switch (nsclient::windows_acl::inspect_protection(shared, state_errors)) {
          case nsclient::windows_acl::protection::restricted:
            LOG_DEBUG_CORE(shared + " is already restricted to SYSTEM and Administrators");
            return;
          case nsclient::windows_acl::protection::unknown:
            // Reading the security descriptor needs READ_CONTROL, which the
            // lockdown denies to everyone else - so being unable to look is
            // itself evidence that the folder is not open, and there is nothing
            // useful an unprivileged process could do about it either way.
            for (const std::string &e : state_errors) LOG_DEBUG_CORE("acl: " + e);
            LOG_DEBUG_CORE("Cannot inspect the permissions on " + shared + "; leaving that to the service");
            return;
          case nsclient::windows_acl::protection::open:
            break;  // genuinely wide open - fix it, and complain if we cannot
        }
      }

      boost::filesystem::create_directories(shared, ec);
      if (ec) {
        LOG_ERROR_CORE("Failed to create " + shared + ": " + ec.message());
        return;
      }
      std::list<std::string> errors;
      if (!nsclient::windows_acl::protect_directory(shared, errors)) {
        for (const std::string &e : errors) LOG_ERROR_CORE("acl: " + e);
        // Loud, and deliberately not fatal: refusing to start would take a
        // working agent offline over a permissions problem an operator can fix.
        // Say plainly what is exposed instead - and it is exposed, not "may
        // be": we either just created the folder (which inherits
        // Users: Read & Execute from %ProgramData%) or looked and found it open.
        LOG_ERROR_CORE("Could not restrict access to " + shared +
                       " - it is readable by every user on this machine, including the configuration and the fleet identity.");
        return;
      }
      errors.clear();
      if (!nsclient::windows_acl::is_protected(shared, errors)) {
        // Setting the DACL and the DACL actually excluding everyone else are
        // different claims; check the second one rather than assume it.
        for (const std::string &e : errors) LOG_ERROR_CORE("acl: " + e);
        LOG_ERROR_CORE("Access to " + shared + " is wider than SYSTEM and Administrators after locking it down.");
        return;
      }
      LOG_DEBUG_CORE("Restricted " + shared + " to SYSTEM and Administrators");
    } catch (const std::exception &e) {
      LOG_ERROR_CORE(std::string("Failed to prepare the shared folder: ") + e.what());
    }
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
      tags_(new nsclient::core::tag_repository()) {
  provider_ = new nscp_settings_provider(path_, log_instance_);
  log_instance_->startup();
}

NSClientT::~NSClientT() {
  try {
    delete provider_;
    log_instance_->destroy();
  } catch (...) {
    std::cerr << "UNknown exception raised: When destroying logger" << std::endl;
  }
}

namespace sh = nscapi::settings_helper;

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
      ("settings", "Settings", "Core configuration.")
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
        .add_string("archive folder", sh::path_key(&crash_folder, CRASH_ARCHIVE_FOLDER), "CRASH ARCHIVE LOCATION", "The folder to archive crash dumps in");

    settings.register_all();
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
    settings_manager::get_core()->register_key(0xffff, "/settings/core", "string", "settings maintenance interval", "Maintenance interval",
                                               "How often settings shall reload config if it has changed", "5m", true, false);
    std::string smi = settings_manager::get_settings()->get_string("/settings/core", "settings maintenance interval", "5m");
    try {
      scheduler_.add_task(task_scheduler::schedule_metadata::SETTINGS, smi);
    } catch (const std::exception &e) {
      LOG_ERROR_CORE_STD("Invalid 'settings maintenance interval' value '" + smi + "', falling back to '5m': " + utf8::utf8_from_native(e.what()));
      scheduler_.add_task(task_scheduler::schedule_metadata::SETTINGS, "5m");
    }
    settings_manager::get_core()->register_key(0xffff, "/settings/core", "string", "metrics interval", "Maintenance interval",
                                               "How often to fetch metrics from modules", "10s", true, false);
    smi = settings_manager::get_settings()->get_string("/settings/core", "metrics interval", "10s");
    try {
      scheduler_.add_task(task_scheduler::schedule_metadata::METRICS, smi);
    } catch (const std::exception &e) {
      LOG_ERROR_CORE_STD("Invalid 'metrics interval' value '" + smi + "', falling back to '10s': " + utf8::utf8_from_native(e.what()));
      scheduler_.add_task(task_scheduler::schedule_metadata::METRICS, "10s");
    }
    settings_manager::get_core()->register_key(0xffff, "/settings/core", "int", "settings maintenance threads", "Maintenance thread count",
                                               "How many threads will run in the background to maintain the various core helper tasks.", "1", true, false);
    int count = str::stox<int>(settings_manager::get_settings()->get_string("/settings/core", "settings maintenance threads", "1"));
    scheduler_.set_threads(count);
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
    boot_fleet_sync();
  }
  LOG_DEBUG_CORE(utf8::cvt<std::string>(APPLICATION_NAME " - " CURRENT_SERVICE_VERSION " Started!"));
  return true;
}

// Start the fleet configuration sync loop - but only when this host has been
// enrolled, i.e. the enrollment manifest written by `nscp enroll` exists.
// Un-enrolled hosts pay nothing: no thread, no traffic, just a debug line.
void NSClientT::boot_fleet_sync() {
#ifdef HAVE_ONBOARDING
  try {
    const std::string path = "/settings/fleet";
    settings_manager::get_core()->register_path(0xffff, path, "Fleet configuration sync",
                                                "Settings for keeping this host in sync with an NSClient fleet server (active once the host is "
                                                "enrolled with `nscp enroll`).",
                                                true, false);
    const auto reg_key = [&path](const char *key, const char *title, const char *description, const char *def) {
      settings_manager::get_core()->register_key(0xffff, path, key, "string", title, description, def, true, false);
      return settings_manager::get_settings()->get_string(path, key, def);
    };

    fleet_config config;
    config.state_file = path_->expand_path(reg_key("state file", "State file",
                                                   "The enrollment manifest written by `nscp enroll` (certificates, keys and server urls). "
                                                   "Fleet sync only runs when this file exists.",
                                                   DEFAULT_FLEET_STATE_LOCATION));
    config.managed_path =
        path_->expand_path(reg_key("managed path", "Managed path",
                                   "Directory where the synced configuration (fleet.ini), scripts and the bundle cache are kept.", "${" FLEET_FOLDER_KEY "}"));
    config.hostname = socket_helpers::expand_hostname(
        reg_key("hostname", "Hostname", "Hostname reported as a tag to the fleet server. Set to auto (default) to use this machine's hostname.", "auto"));
    config.tls_version = reg_key("tls version", "TLS version", "The TLS version used when connecting to the fleet server.", "tlsv1.2+");
    const std::string timeout = reg_key("timeout", "Request timeout",
                                        "How long a single request to the fleet server may take before it is abandoned and retried. A server that "
                                        "accepts the connection and then stops responding would otherwise stall the sync loop indefinitely.",
                                        "60s");
    try {
      config.timeout_seconds = static_cast<unsigned int>(str::format::stox_as_time_sec<long>(timeout, "s"));
    } catch (const std::exception &e) {
      LOG_ERROR_CORE_STD("Invalid fleet 'timeout' value '" + timeout + "', falling back to 60s: " + utf8::utf8_from_native(e.what()));
    }
    config.nscp_version = CURRENT_SERVICE_VERSION;
    config.local_config_probe = [] { return settings_manager::has_local_configuration(); };

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
    const std::shared_ptr<fleet_sync> sync = std::make_shared<fleet_sync>(log_instance_, config, tags_, [this] { this->reload("delayed,service"); });
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

void NSClientT::unloadPlugins() {
  log_instance_->clear_subscribers();
  plugins_->stop_plugins();
}
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
  // TODO: a module *disabled* since the last load is still left running; that
  // needs unloading a live plugin, which is a different problem from this one.
  settings_manager::get_core()->set_reload(false);
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
    std::string task = module;
    bool delayed = false;
    if (module.size() > 8 && module.substr(0, 8) == "delayed,") {
      task = module.substr(8);
      delayed = true;
    } else if (module.size() > 6 && module.substr(0, 6) == "delay,") {
      task = module.substr(6);
      delayed = true;
    } else if (module.size() > 6 && module.substr(0, 8) == "instant,") {
      task = module.substr(8);
      delayed = false;
    } else if (module == "service") {
      delayed = false;
    }
    if (delayed) {
      LOG_TRACE_CORE("Delayed reload");
      scheduler_.add_task(task_scheduler::schedule_metadata::RELOAD, "", task);
      return NSCAPI::api_return_codes::isSuccess;
    } else {
      LOG_TRACE_CORE("Instant reload");
      return do_reload(task) ? NSCAPI::api_return_codes::isSuccess : NSCAPI::api_return_codes::hasFailed;
    }

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
  PB::Metrics::MetricsBundle bundle;
  bundle.set_key("workers");
  if (scheduler_.get_scheduler().has_metrics()) {
    boost::uint64_t taskes_ = scheduler_.get_scheduler().get_metric_executed();
    boost::uint64_t submitted_ = scheduler_.get_scheduler().get_metric_compleated();
    boost::uint64_t errors_ = scheduler_.get_scheduler().get_metric_errors();
    boost::uint64_t threads = scheduler_.get_scheduler().get_metric_threads();

    PB::Metrics::Metric *m = bundle.add_value();
    m->set_key("jobs");
    m->mutable_gauge_value()->set_value(static_cast<double>(taskes_));
    m = bundle.add_value();
    m->set_key("submitted");
    m->mutable_gauge_value()->set_value(static_cast<double>(submitted_));
    m = bundle.add_value();
    m->set_key("errors");
    m->mutable_gauge_value()->set_value(static_cast<double>(errors_));
    m = bundle.add_value();
    m->set_key("threads");
    m->mutable_gauge_value()->set_value(static_cast<double>(threads));
    m = bundle.add_value();
    m->set_key("refresh_interval");
    m->mutable_gauge_value()->set_value(scheduler_.get_metrics_interval());

  } else {
    PB::Metrics::Metric *m = bundle.add_value();
    m->set_key("metrics.available");
    m->mutable_gauge_value()->set_value(0);
  }
  return bundle;
}
void NSClientT::process_metrics() { plugins_->process_metrics(ownMetricsFetcher()); }

#ifdef _WIN32
void NSClientT::handle_session_change(unsigned long dwSessionId, bool logon) {}
#endif
