// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <boost/algorithm/string.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/optional.hpp>
#include <boost/thread/recursive_mutex.hpp>
#include <memory>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/protobuf/metrics.hpp>
#include <nsclient/logger/logger.hpp>

#include "../channels.hpp"
#include "../commands.hpp"
#include "../fact_repository.hpp"
#include "../path_manager.hpp"
#include "../permissions.hpp"
#include "../routers.hpp"
#include "master_plugin_list.hpp"
#include "plugin_cache.hpp"

/**
 * Main NSClient++ core class. This is the service core and as such is responsible for pretty much everything.
 * It also acts as a broker for all plugins and other sub threads and such.
 *
 * @version 1.0
 * first version
 *
 * @date 02-12-2005
 *
 * @todo Plugininfy the socket somehow ?
 * It is technically possible to make the socket a plug-in but would it be a good idea ?
 */

namespace nsclient {
namespace core {

class core_exception : public std::exception {
  std::string what_;

 public:
  explicit core_exception(const std::string &error) throw() : what_(error.c_str()) {}
  ~core_exception() throw() override {};

  const char *what() const throw() override { return what_.c_str(); }
};

class plugin_manager : public std::enable_shared_from_this<plugin_manager> {
 public:
  typedef std::shared_ptr<plugin_interface> plugin_type;

 private:
  boost::filesystem::path plugin_path_;

  path_instance path_;
  logging::logger_instance log_instance_;
  master_plugin_list plugin_list_;
  commands commands_;
  channels channels_;
  simple_plugins_list metrics_fetchers_;
  simple_plugins_list metrics_submitters_;
  simple_plugins_list facts_fetchers_;
  // The core's fact repository, handed over at boot. Held here so a plugin
  // leaving drops its fact sets in the same place it is dropped from every
  // other registry - the alternative is a `docker` set that survives
  // unloading CheckDocker and freezes at whatever the socket last said.
  fact_repository_instance facts_;
  plugin_cache plugin_cache_;
  event_subscribers event_subscribers_;
  permissions permissions_;
  // Serialises whole plugin lifecycle operations - load, reload, unload,
  // purge, start and stop - against each other. The individual registries are
  // each locked, but a lifecycle operation is a sequence of steps across all
  // of them, and two of those running at once interleave: a web request
  // loading a module while the scheduler reloads, or two queued reloads on
  // different workers. Recursive because these call one another
  // (load_single_plugin -> add_plugin, start_plugins -> purge_broken_plugin).
  boost::recursive_mutex lifecycle_mutex_;

 public:
  plugin_manager(path_instance path_, logging::logger_instance log_instance);
  virtual ~plugin_manager();

  plugin_cache *get_plugin_cache() { return &plugin_cache_; }
  void set_fact_repository(fact_repository_instance facts) { facts_ = std::move(facts); }
  commands *get_commands() { return &commands_; }
  channels *get_channels() { return &channels_; }
  event_subscribers *get_event_subscribers() { return &event_subscribers_; }
  permissions *get_permissions() { return &permissions_; }

  // Read /settings/permissions{,/policies} into the in-memory policy
  // table. Called once at boot (from NSClient++.cpp after settings come
  // up) and again from do_reload("settings") so operators can update
  // policies without restarting the service. See
  // docs/design/core-permissions.md for the wire format.
  void load_permissions();

  void set_path(boost::filesystem::path path);

  void load_active_plugins();
  void load_all_plugins();
  bool load_single_plugin(const std::string &plugin, const std::string &alias = "", bool start = false);
  void start_plugins(NSCAPI::moduleLoadMode mode);
  void post_start_plugins();

 private:
  // Drop every registry reference to a plugin that failed to load or start
  // and unload it. A shared_ptr left behind in any registry keeps the
  // dll_plugin alive past shutdown, and its destructor then calls back into
  // the module DSO at static-destruction time, after the DSO's own statics
  // are gone — a crash on process exit.
  void purge_broken_plugin(unsigned long plugin_id);

 public:
  void prepare_shutdown_plugins();
  void stop_plugins();
  plugin_type only_load_module(const std::string &module, const std::string &alias, bool &loaded);

  plugin_type find_plugin(const unsigned int plugin_id);
  bool remove_plugin(const std::string &name);
  int clone_plugin(unsigned int plugin_id);
  bool reload_plugin(const std::string &module);

  typedef boost::function<int(plugin_type)> run_function;
  int load_and_run(std::string module, run_function fun, std::list<std::string> &errors);
  NSCAPI::errorReturn send_notification(const char *channel, std::string &request, std::string &response);
  NSCAPI::nagiosReturn execute_query(const std::string &request, std::string &response);
  ::PB::Commands::QueryResponseMessage execute_query(const ::PB::Commands::QueryRequestMessage &);
  std::wstring execute(std::wstring password, std::wstring cmd, std::list<std::wstring> args);
  int simple_exec(std::string command, std::vector<std::string> arguments, std::list<std::string> &resp);
  int simple_query(std::string module, std::string command, std::vector<std::string> arguments, std::list<std::string> &resp);
  NSCAPI::nagiosReturn exec_command(const char *target, std::string request, std::string &response);
  void register_submission_listener(unsigned int plugin_id, const char *channel);
  NSCAPI::nagiosReturn emit_event(const std::string &request);

  bool is_enabled(const std::string module);
  // Fetch metrics from every fetcher, append `bundle` (the core's own) and hand
  // the result to every submitter. Returns the assembled message so a caller can
  // feed non-plugin consumers from the same snapshot.
  PB::Metrics::MetricsMessage process_metrics(PB::Metrics::MetricsBundle bundle);

  // Run one facts round: ask every facts producer for the enabled fact sets
  // and apply what comes back to `repository`. `reason` is `startup`,
  // `scheduled`, `reload` or `manual` and reaches the producers unchanged, so
  // one with an expensive collection can skip work it knows has not moved.
  //
  // The enabled list is authoritative and is resolved here, not in the
  // producers: a set nobody enabled is ignored even if a module returns it,
  // and a set that was turned off is dropped from the repository before the
  // round runs. That is the whole of "nothing is collected until a fact set is
  // enabled", in one place rather than in every module.
  void process_facts(const std::vector<std::string> &enabled, const std::string &reason);

  // Whether any module can produce facts at all. The scheduler uses it to
  // avoid registering a round nobody would answer.
  bool has_facts_fetchers();

  bool enable_plugin(std::string name);
  bool disable_plugin(std::string name);

 private:
  typedef std::multimap<std::string, std::string> plugin_alias_list_type;

  boost::optional<boost::filesystem::path> find_file(const std::string &file_name);
  bool contains_plugin(plugin_alias_list_type &ret, std::string alias, std::string plugin);
  std::string get_plugin_module_name(unsigned int plugin_id);

  plugin_type add_plugin(const std::string &file_name, const std::string &alias);

  plugin_alias_list_type find_all_plugins();
  plugin_alias_list_type find_all_active_plugins();
  logging::logger_instance get_logger() { return log_instance_; }
  struct plugin_status {
    std::string alias;
    std::string plugin;
    bool enabled;

    plugin_status(std::string alias, std::string plugin, bool enabled) : alias(alias), plugin(plugin), enabled(enabled) {}
    plugin_status(std::string plugin) : alias(""), plugin(plugin), enabled(true) {}
    plugin_status(const plugin_status &other) : alias(other.alias), plugin(other.plugin), enabled(other.enabled) {}

    plugin_status &operator=(const plugin_status &other) {
      this->alias = other.alias;
      this->plugin = other.plugin;
      this->enabled = other.enabled;
      return *this;
    }
  };
  plugin_status parse_plugin(std::string key);

  static std::string get_plugin_file(const std::string &key) {
#ifdef WIN32
    return key + ".dll";
#else
    return "lib" + key + ".so";
#endif
  }

 public:
  // Resolve the caller subject (`module[:principal]`) from a request
  // header's metadata. Exposed as a public static so the policy decision
  // point in execute_query / exec_command can call it, and so unit tests
  // can exercise it without spinning up a full plugin_manager.
  //
  // Reads `nscp.caller_plugin_id` (stamped by nscapi::core_helper) and
  // resolves it to a module name via the trusted plugin_cache. Reads
  // `nscp.principal` verbatim. Returns "*" for the module when the
  // plugin id is missing or unresolved, so a strict allow-list catches
  // the unknown-caller case explicitly.
  static std::string extract_subject_from_header(const PB::Common::Header &header, plugin_cache *cache);

  static bool is_module(const boost::filesystem::path &file) {
#ifdef WIN32
    return boost::ends_with(file.string(), ".dll");
#else
    return boost::ends_with(file.string(), ".so");
#endif
  }
  static boost::filesystem::path get_filename(boost::filesystem::path folder, std::string module);
  static std::string file_to_module(const boost::filesystem::path &file) {
    std::string str = file.string();
#ifdef WIN32
    if (boost::ends_with(str, ".dll")) str = str.substr(0, str.size() - 4);
#else
    if (boost::ends_with(str, ".so")) str = str.substr(0, str.size() - 3);
    if (boost::starts_with(str, "lib")) str = str.substr(3);
#endif
    return str;
  }
};

typedef std::shared_ptr<plugin_manager> plugin_mgr_instance;

}  // namespace core
}  // namespace nsclient
