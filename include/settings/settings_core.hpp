/*
 * Copyright (C) 2004-2016 Michael Medin
 *
 * This file is part of NSClient++ - https://nsclient.org
 *
 * NSClient++ is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * NSClient++ is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with NSClient++.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <settings/settings_interface.hpp>

#include <nsclient/logger/logger.hpp>

#include <boost/filesystem/path.hpp>

#include <string>
#include <map>
#include <set>

#define BUFF_LEN 4096

namespace settings {
inline std::string key_to_string(std::string path, std::string key) { return path + "." + key; }

inline std::string join_path(const std::string &p1, const std::string &p2) {
  if (p1.size() > 0 && p1[p1.size() - 1] == '/' && p2.size() > 1 && p2[0] == '/') {
    // .../, /...
    return p1 + p2.substr(1);
  } else if (p1.size() > 0 && p1[p1.size() - 1] == '/') {
    // .../, ...
    return p1 + p2;
  } else if (p2.size() > 1 && p2[0] == '/') {
    // ..., /...
    return p1 + p2;
  }
  // ..., ...
  return p1 + "/" + p2;
}

class settings_core {
 public:
  typedef std::list<std::string> string_list;
  typedef std::pair<std::string, std::string> key_path_type;
  struct key_description {
    std::string title;
    std::string description;
    std::string default_value;
    bool advanced;
    bool is_sample;
    std::set<unsigned int> plugins;
    key_description(unsigned int plugin_id, std::string title_, std::string description_, std::string default_value, bool advanced_, bool is_sample_)
        : title(title_), description(description_), default_value(default_value), advanced(advanced_), is_sample(is_sample_) {
      append_plugin(plugin_id);
    }
    key_description(unsigned int plugin_id) : advanced(false), is_sample(false) { append_plugin(plugin_id); }
    key_description() : advanced(false), is_sample(false) {}
    key_description &operator=(const key_description &other) {
      title = other.title;
      description = other.description;
      default_value = other.default_value;
      advanced = other.advanced;
      is_sample = other.is_sample;
      plugins = other.plugins;
      return *this;
    }
    bool has_plugin(unsigned int plugin_id) const { return plugins.find(plugin_id) != plugins.end(); }
    void append_plugin(unsigned int plugin_id) { plugins.insert(plugin_id); }
  };

  struct subkey_description {
    bool is_subkey;
    std::string title;
    std::string description;
    bool advanced;
    bool is_sample;
    subkey_description(std::string title_, std::string description_, bool advanced_, bool is_sample_)
        : is_subkey(true), title(title_), description(description_), advanced(advanced_), is_sample(is_sample_) {}
    subkey_description() : is_subkey(false), advanced(false), is_sample(false) {}
    void update(std::string title_, std::string description_, bool advanced_, bool is_sample_) {
      is_subkey = true;
      title = title_;
      description = description_;
      advanced = advanced_;
      is_sample = is_sample_;
    }
  };
  struct path_description {
    std::string title;
    std::string description;
    bool advanced;
    bool is_sample;
    subkey_description subkey;
    typedef std::map<std::string, key_description> keys_type;
    keys_type keys;
    std::set<unsigned int> plugins;
    path_description(unsigned int plugin_id, std::string title_, std::string description_, bool advanced_, bool is_sample_)
        : title(title_), description(description_), advanced(advanced_), is_sample(is_sample_) {
      append_plugin(plugin_id);
    }
    path_description(unsigned int plugin_id) : advanced(false), is_sample(false) { append_plugin(plugin_id); }
    path_description() : advanced(false), is_sample(false) {}
    void update(unsigned int plugin_id, std::string title_, std::string description_, bool advanced_, bool is_sample_) {
      title = title_;
      description = description_;
      advanced = advanced_;
      is_sample = is_sample_;
      append_plugin(plugin_id);
    }
    void append_plugin(unsigned int plugin_id) { plugins.insert(plugin_id); }
  };

  struct tpl_description {
    unsigned int plugin_id;
    std::string path;
    std::string title;
    std::string data;

    tpl_description() : plugin_id(0) {}
    tpl_description(unsigned int plugin_id, std::string path, std::string title, std::string data)
        : plugin_id(plugin_id), path(path), title(title), data(data) {}
  };

  struct mapped_key {
    mapped_key(key_path_type src_, key_path_type dst_) : src(src_), dst(dst_) {}
    key_path_type src;
    key_path_type dst;
  };
  typedef std::list<mapped_key> mapped_key_list_type;

  //////////////////////////////////////////////////////////////////////////
  /// Register a path with the settings module.
  /// A registered key or path will be nicely documented in some of the settings files when converted.
  ///
  /// @param path The path to register
  /// @param title The title to use
  /// @param description the description to use
  /// @param advanced advanced options will only be included if they are changed
  ///
  /// @author mickem
  virtual void register_path(unsigned int plugin_id, std::string path, std::string title, std::string description, bool advanced, bool is_sample,
                             bool update_existing = true) = 0;

  virtual void register_subkey(unsigned int plugin_id, std::string path, std::string title, std::string description, bool advanced, bool is_sample,
                               bool update_existing = true) = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Register a key with the settings module.
  /// A registered key or path will be nicely documented in some of the settings files when converted.
  ///
  /// @param path The path to register
  /// @param key The key to register
  /// @param type The type of value
  /// @param title The title to use
  /// @param description the description to use
  /// @param defValue the default value
  /// @param advanced advanced options will only be included if they are changed
  ///
  /// @author mickem
  virtual void register_key(unsigned int plugin_id, std::string path, std::string key, std::string title, std::string description, std::string defValue,
                            bool advanced, bool is_sample, bool update_existing = true) = 0;
  virtual void add_sensitive_key(unsigned int plugin_id, std::string path, std::string key) = 0;

  virtual void register_tpl(unsigned int plugin_id, std::string path, std::string title, std::string data) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Get info about a registered key.
  /// Used when writing settings files.
  ///
  /// @param path The path of the key
  /// @param key The key of the key
  /// @return the key description
  ///
  /// @author mickem
  virtual boost::optional<key_description> get_registered_key(std::string path, std::string key) = 0;
  virtual bool is_sensitive_key(std::string path, std::string key) = 0;

  virtual settings_core::path_description get_registered_path(const std::string &path) = 0;

  virtual std::list<settings_core::tpl_description> get_registered_templates() = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Get all registered sections
  ///
  /// @return a list of section paths
  ///
  /// @author mickem
  virtual string_list get_reg_sections(std::string path, bool fetch_samples) = 0;
  //////////////////////////////////////////////////////////////////////////
  /// Get all keys for a registered section.
  ///
  /// @param path the path to find keys under
  /// @return a list of key names
  ///
  /// @author mickem
  virtual string_list get_reg_keys(std::string path, bool fetch_samples) = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Get the currently active settings interface.
  ///
  /// @return the currently active interface
  ///
  /// @author mickem
  virtual instance_ptr get() = 0;
  virtual instance_ptr get_no_wait() = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Get a settings interface
  ///
  /// @param type the type of settings interface to get
  /// @return the settings interface
  ///
  /// @author mickem
  // virtual settings_interface* get(settings_core::settings_type type) = 0;
  //  Conversion Functions
  virtual void migrate_to(std::string alias, std::string to) = 0;
  virtual void migrate_from(std::string alias, std::string from) = 0;

  virtual void set_primary(std::string context) = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Validate the settings store and report all missing/invalid and superflous keys.
  ///
  /// @author mickem
  virtual settings::error_list validate() = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Overwrite the (current) settings store with default values.
  ///
  /// @author mickem
  virtual void update_defaults() = 0;
  virtual void remove_defaults() = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Boot the settings subsystem from the given file (boot.ini).
  ///
  /// @param file the file to use when booting.
  ///
  /// @author mickem
  virtual void boot(std::string file = "boot.ini") = 0;
  virtual void set_ready(bool is_read = true) = 0;
  virtual bool is_ready() = 0;

  virtual void house_keeping() = 0;

  virtual std::string find_file(std::string file, std::string fallback) = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Create an instance of a given type.
  /// Used internally to create instances of various settings types.
  ///
  /// @param context the context to use
  /// @return a new instance of given type.
  ///
  /// @author mickem
  virtual instance_raw_ptr create_instance(std::string alias, std::string context) = 0;

  //////////////////////////////////////////////////////////////////////////
  /// Set the basepath for the settings subsystem.
  /// In other words set where the settings files reside
  ///
  /// @param path the path to the settings files
  ///
  /// @author mickem
  virtual void set_base(boost::filesystem::path path) = 0;

  virtual std::string to_string() = 0;

  virtual std::string expand_path(std::string key) = 0;

  virtual std::string expand_context(const std::string &key) = 0;

  virtual void set_dirty(bool flag = true) = 0;
  virtual bool is_dirty() = 0;
  virtual void set_reload(bool flag = true) = 0;
  virtual bool needs_reload() = 0;

  virtual bool supports_updates() = 0;
  virtual bool use_sensitive_keys() = 0;

  virtual nsclient::logging::logger_instance get_logger() const = 0;
};

}  // namespace settings
