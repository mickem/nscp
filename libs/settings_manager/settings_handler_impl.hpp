// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <atomic>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/thread/locks.hpp>
#include <boost/thread/thread.hpp>
#include <map>
#include <nsclient/logger/logger.hpp>
#include <set>
#include <settings/settings_core.hpp>
#include <string>

namespace settings {
class settings_handler_impl : public settings_core {
 private:
  typedef std::map<std::string, std::string> path_map;
  typedef std::map<std::string, settings_core::path_description> reg_paths_type;
  typedef std::map<key_path_type, key_path_type> mapped_paths_type;
  typedef std::map<std::string, tpl_description> tpl_desc_type;
  typedef settings_interface::string_list string_list;

  instance_raw_ptr instance_;
  boost::timed_mutex instance_mutex_;
  boost::filesystem::path base_path_;

  boost::shared_mutex registry_mutex_;
  // Keyed on path + separator + key: sensitivity is per (path, key), never
  // per key name, so a flag on /a."password" must not reach /b."password".
  static std::string make_sensitive_key(const std::string &path, const std::string &key) { return path + "|||" + key; }
  std::set<std::string> sensitive_keys_;
  reg_paths_type registred_paths_;
  tpl_desc_type registered_tpls_;
  nsclient::logging::logger_instance logger_;
  // Three flags with three different writers and three different readers:
  // set_reload(true) from the http instance's house_keeping on a scheduler
  // worker, set_reload(false) from reloadPlugins() / clear_cache() on another
  // worker or on the web thread, set_dirty() from every web `settings --set`,
  // and is_dirty() read by the web status query. Atomics, so a reload request
  // is neither missed nor seen twice.
  std::atomic<bool> ready_flag;
  std::atomic<bool> dirty_flag;
  std::atomic<bool> reload_flag;

 public:
  settings_handler_impl(nsclient::logging::logger_instance logger) : logger_(logger), ready_flag(false), dirty_flag(false), reload_flag(false) {
    // Credentials the core owns rather than any single module. The password
    // under /settings/default is the shared secret NRPE, NSCA, NSClient and
    // the web server all fall back to, but only those modules declare it with
    // add_password - so on an agent running none of them (check modules only,
    // or NRPE alone) nothing marked it sensitive: `settings show` printed it
    // in the clear and `settings --update` left it in the INI rather than
    // moving it to the credential store. Sensitivity is a property of the
    // key, not of which consumer happens to be enabled, so seed it here.
    sensitive_keys_.emplace(make_sensitive_key("/settings/default", "password"));
  }
  virtual ~settings_handler_impl() { destroy_all_instances(); }
  bool is_ready() { return ready_flag; }
  void set_ready(bool flag = true) { ready_flag = flag; }
  bool is_dirty() { return dirty_flag; }
  void set_dirty(bool flag = true) { dirty_flag = flag; }
  void set_reload(bool flag = true) { reload_flag = flag; }
  bool needs_reload() { return reload_flag; }

  //////////////////////////////////////////////////////////////////////////
  /// Set the basepath for the settings subsystem.
  /// In other words set where the settings files reside
  ///
  /// @param path the path to the settings files
  void set_base(boost::filesystem::path path) { base_path_ = path; }

  //////////////////////////////////////////////////////////////////////////
  /// Get the logging interface (will receive log messages)
  ///
  /// @return the logger to use
  nsclient::logging::logger_instance get_logger() const { return logger_; }

  //////////////////////////////////////////////////////////////////////////
  /// Get the basepath for the settings subsystem.
  /// In other words get where the settings files reside
  ///
  /// @return the path to the settings files
  boost::filesystem::path get_base() { return base_path_; }

  settings::error_list validate();

  void house_keeping();

  instance_ptr get();
  instance_ptr get_no_wait();
  void update_defaults(bool include_samples = false);
  void remove_defaults();
  // primary_context is what set_primary stores in boot.ini. It is passed
  // separately from `to` because an instance's context is the *expanded* one
  // (create_instance resolves host name placeholders so it can open the per-
  // host file), and storing that would bake this host's name into a boot.ini
  // meant for every machine (issue #458): the callers which still have the
  // context as the operator wrote it hand that in, placeholder intact, and
  // set_primary resolves the protocol aliases itself.
  void migrate(instance_ptr from, instance_ptr to, const std::string &primary_context) {
    if (!from || !to) throw settings_exception(__FILE__, __LINE__, "Source or target is null");
    from->save_to(to);
    set_primary(primary_context);
  }
  void migrate(instance_ptr from, instance_ptr to) {
    if (!to) throw settings_exception(__FILE__, __LINE__, "Source or target is null");
    migrate(from, to, to->get_context());
  }
  void migrate_to(instance_ptr to) { migrate(get(), to); }
  void migrate_from(instance_ptr from) { migrate(from, get()); }
  void migrate_to(std::string alias, std::string to) {
    instance_ptr i = create_instance(alias, to);
    migrate(get(), i, to);
  }
  void migrate_from(std::string alias, std::string from) {
    instance_ptr i = create_instance(alias, from);
    migrate_from(i);
  }
  void migrate(std::string alias_from, std::string from, std::string alias_to, std::string to) {
    instance_ptr ifrom = create_instance(alias_from, from);
    instance_ptr ito = create_instance(alias_to, to);
    migrate(ifrom, ito, to);
  }

  //////////////////////////////////////////////////////////////////////////
  /// Register a path with the settings module.
  /// A registered key or path will be nicely documented in some of the settings files when converted.
  ///
  /// @param path The path to register
  /// @param type The type of value
  /// @param title The title to use
  /// @param description the description to use
  /// @param defValue the default value
  /// @param advanced advanced options will only be included if they are changed
  void register_path(unsigned int plugin_id, std::string path, std::string title, std::string description, bool advanced, bool is_sample,
                     bool update_existing) {
    boost::unique_lock<boost::shared_mutex> writeLock(registry_mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
    if (!writeLock.owns_lock()) {
      throw settings_exception(__FILE__, __LINE__, "Failed to lock registry mutex: " + path);
    }
    reg_paths_type::iterator it = registred_paths_.find(path);
    if (it == registred_paths_.end()) {
      registred_paths_[path] = path_description(plugin_id, title, description, advanced, is_sample);
    } else if (update_existing) {
      (*it).second.update(plugin_id, title, description, advanced, is_sample);
    }
  }

  //////////////////////////////////////////////////////////////////////////
  /// Register a path with the settings module.
  /// A registered key or path will be nicely documented in some of the settings files when converted.
  ///
  /// @param path The path to register
  /// @param type The type of value
  /// @param title The title to use
  /// @param description the description to use
  /// @param defValue the default value
  /// @param advanced advanced options will only be included if they are changed
  void register_subkey(unsigned int plugin_id, std::string path, std::string title, std::string description, bool advanced, bool is_sample,
                       bool update_existing) {
    boost::unique_lock<boost::shared_mutex> writeLock(registry_mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
    if (!writeLock.owns_lock()) {
      throw settings_exception(__FILE__, __LINE__, "Failed to lock registry mutex: " + path);
    }
    reg_paths_type::iterator it = registred_paths_.find(path);
    if (it == registred_paths_.end()) {
      registred_paths_[path] = path_description(plugin_id, title, description, advanced, is_sample);
      registred_paths_[path].subkey = subkey_description(title, description, advanced, is_sample);
    } else {
      if (!registred_paths_[path].subkey.is_subkey || update_existing) {
        registred_paths_[path].subkey = subkey_description(title, description, advanced, is_sample);
      }
    }
  }

  //////////////////////////////////////////////////////////////////////////
  /// Register a key with the settings module.
  /// A registered key or path will be nicely documented in some of the settings files when converted.
  ///
  /// @param path The path to register
  /// @param key The key to register
  /// @param title The title to use
  /// @param description the description to use
  /// @param defValue the default value
  /// @param advanced advanced options will only be included if they are changed
  void register_key(unsigned int plugin_id, std::string path, std::string key, std::string type, std::string title, std::string description,
                    std::string defValue, bool advanced, bool is_sample, bool update_existing = true) {
    boost::unique_lock<boost::shared_mutex> writeLock(registry_mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
    if (!writeLock.owns_lock()) {
      throw settings_exception(__FILE__, __LINE__, "Failed to lock registry mutex: " + path + "." + key);
    }
    reg_paths_type::iterator it = registred_paths_.find(path);
    if (it == registred_paths_.end()) {
      registred_paths_[path] = path_description(plugin_id, "", "", false, is_sample);
      registred_paths_[path].keys[key] = key_description(plugin_id, type, title, description, defValue, advanced, is_sample);
    } else if (update_existing) {
      (*it).second.append_plugin(plugin_id);
      path_description::keys_type::iterator kit = (*it).second.keys.find(key);
      if (kit == (*it).second.keys.end()) {
        (*it).second.keys[key] = key_description(plugin_id, type, title, description, defValue, advanced, is_sample);
      } else {
        (*kit).second.append_plugin(plugin_id);
        if (!description.empty() && (*kit).second.description.empty()) {
          (*kit).second.description = description;
        }
      }
    }
  }
  void add_sensitive_key(unsigned int _plugin_id, std::string path, std::string key) override {
    boost::unique_lock<boost::shared_mutex> writeLock(registry_mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
    if (!writeLock.owns_lock()) {
      throw settings_exception(__FILE__, __LINE__, "Failed to lock registry mutex: " + path + "." + key);
    }
    sensitive_keys_.emplace(make_sensitive_key(path, key));
  }

  void register_tpl(unsigned int plugin_id, std::string path, std::string title, std::string data) {
    boost::unique_lock<boost::shared_mutex> writeLock(registry_mutex_, boost::get_system_time() + boost::posix_time::seconds(10));
    if (!writeLock.owns_lock()) throw settings_exception(__FILE__, __LINE__, "Failed to get mutex for register_tpl");
    std::string key = path + "::" + title;
    registered_tpls_[key] = tpl_description(plugin_id, path, title, data);
  }

  //////////////////////////////////////////////////////////////////////////
  /// Get info about a registered key.
  /// Used when writing settings files.
  ///
  /// @param path The path of the key
  /// @param key The key of the key
  /// @return the key description
  boost::optional<settings_core::key_description> get_registered_key(std::string path, std::string key) {
    boost::shared_lock<boost::shared_mutex> readLock(registry_mutex_, boost::get_system_time() + boost::posix_time::milliseconds(5000));
    if (!readLock.owns_lock()) {
      throw settings_exception(__FILE__, __LINE__, "Failed to lock registry mutex: " + path + "." + key);
    }
    reg_paths_type::const_iterator cit = registred_paths_.find(path);
    if (cit != registred_paths_.end()) {
      path_description::keys_type::const_iterator cit2 = (*cit).second.keys.find(key);
      if (cit2 != (*cit).second.keys.end()) {
        settings_core::key_description ret = (*cit2).second;
        return ret;
      }
      subkey_description subkey = (*cit).second.subkey;
      if (subkey.is_subkey) {
        return settings_core::key_description(0xffff, "string", subkey.title, subkey.description, "", subkey.advanced, subkey.is_sample);
      }
    }
    if (path == "/modules") {
      return settings_core::key_description(0, "bool", "Load on startup", "If the module should be loaded on startup", "false", false, false);
    }
    return boost::none;
  }
  bool is_sensitive_key(const std::string path, const std::string key) override {
    boost::shared_lock<boost::shared_mutex> readLock(registry_mutex_, boost::get_system_time() + boost::posix_time::milliseconds(5000));
    if (!readLock.owns_lock()) {
      throw settings_exception(__FILE__, __LINE__, "Failed to lock registry mutex: " + path);
    }
    return sensitive_keys_.find(make_sensitive_key(path, key)) != sensitive_keys_.end();
  }
  settings_core::path_description get_registered_path(const std::string &path) {
    boost::shared_lock<boost::shared_mutex> readLock(registry_mutex_, boost::get_system_time() + boost::posix_time::milliseconds(5000));
    if (!readLock.owns_lock()) {
      throw settings_exception(__FILE__, __LINE__, "Failed to lock registry mutex: " + path);
    }
    reg_paths_type::const_iterator cit = registred_paths_.find(path);
    if (cit != registred_paths_.end()) {
      return (*cit).second;
    }
    throw settings_exception(__FILE__, __LINE__, "Path not found: " + path);
  }

  std::list<settings_core::tpl_description> get_registered_templates() {
    std::list<settings_core::tpl_description> ret;
    boost::shared_lock<boost::shared_mutex> readLock(registry_mutex_, boost::get_system_time() + boost::posix_time::milliseconds(5000));
    if (!readLock.owns_lock()) {
      throw settings_exception(__FILE__, __LINE__, "Failed to lock registry mutex: when fetching tpls");
    }
    for (const tpl_desc_type::value_type &d : registered_tpls_) {
      ret.push_back(d.second);
    }
    return ret;
  }

  //////////////////////////////////////////////////////////////////////////
  /// Get all registered sections
  ///
  /// @return a list of section paths
  string_list get_reg_sections(std::string path, bool fetch_samples) {
    boost::shared_lock<boost::shared_mutex> readLock(registry_mutex_, boost::get_system_time() + boost::posix_time::milliseconds(5000));
    if (!readLock.owns_lock()) {
      throw settings_exception(__FILE__, __LINE__, "Failed to lock registry mutex: " + path);
    }
    string_list ret;
    for (const reg_paths_type::value_type &v : registred_paths_) {
      if ((!v.second.is_sample || fetch_samples) && (path.empty() || boost::starts_with(v.first, path))) ret.push_back(v.first);
    }
    return ret;
  }
  //////////////////////////////////////////////////////////////////////////
  /// Get all keys for a registered section.
  ///
  /// @param path the path to find keys under
  /// @return a list of key names
  virtual string_list get_reg_keys(std::string path, bool fetch_samples) {
    boost::shared_lock<boost::shared_mutex> readLock(registry_mutex_, boost::get_system_time() + boost::posix_time::milliseconds(5000));
    if (!readLock.owns_lock()) {
      throw settings_exception(__FILE__, __LINE__, "Failed to lock registry mutex: " + path);
    }
    string_list ret;
    reg_paths_type::const_iterator cit = registred_paths_.find(path);
    if (cit != registred_paths_.end()) {
      for (const path_description::keys_type::value_type &v : (*cit).second.keys) {
        if (!v.second.is_sample || fetch_samples) ret.push_back(v.first);
      }
      return ret;
    }
    return ret;
  }

  void set_instance(std::string alias, std::string key) {
    // Built before the lock is taken: creating an http instance fetches the
    // configuration over the network, and holding instance_mutex_ across that
    // would stall every settings read in the process for as long as the
    // fetch takes.
    instance_raw_ptr instance = create_instance(alias, key);
    if (!instance) throw settings_exception(__FILE__, __LINE__, "set_instance Failed to create instance for: " + key);
    boost::unique_lock<boost::timed_mutex> mutex(instance_mutex_, boost::get_system_time() + boost::posix_time::seconds(5));
    if (!mutex.owns_lock()) throw settings_exception(__FILE__, __LINE__, "set_instance Failed to get mutex, cant get access settings");
    instance_ = instance;
  }

  bool supports_updates() override;
  bool use_sensitive_keys() override;

 private:
  void destroy_all_instances();

  virtual std::string to_string() {
    if (instance_) return instance_->to_string();
    return "<NULL>";
  }
};
typedef settings_interface::string_list string_list;
}  // namespace settings
