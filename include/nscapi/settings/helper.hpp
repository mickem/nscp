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

#include <boost/filesystem/path.hpp>
#include <boost/function.hpp>
#include <boost/shared_ptr.hpp>
#include <list>
#include <map>
#include <settings/client/settings_client_interface.hpp>
#include <string>
#include <utility>

namespace nscapi {
namespace settings_helper {
typedef boost::shared_ptr<settings_impl_interface> settings_impl_interface_ptr;

class key_interface {
 public:
  virtual ~key_interface() = default;
  virtual std::string get_default() const = 0;
  virtual void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const = 0;
  virtual void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const = 0;
  virtual void notify_path(settings_impl_interface_ptr core_, std::string path) const = 0;
};
typedef boost::shared_ptr<key_interface> key_type;

key_type string_key(std::string* val, const std::string& def);
key_type string_key(std::string* val);
key_type int_key(int* val, int def = 0);
key_type size_key(std::size_t* val, std::size_t def = 0);
key_type uint_key(unsigned int* val, unsigned int def);
key_type uint_key(unsigned int* val);
key_type bool_key(bool* val, bool def);
key_type bool_key(bool* val);
key_type path_key(std::string* val, std::string def);
key_type path_key(std::string* val);
key_type path_key(boost::filesystem::path* val, std::string def);
key_type path_key(boost::filesystem::path* val);

key_type string_fun_key(boost::function<void(std::string)> fun, std::string def);
key_type string_fun_key(boost::function<void(std::string)> fun);
key_type cstring_fun_key(boost::function<void(const char*)> fun);
key_type cstring_fun_key(boost::function<void(const char*)> fun, std::string def);
key_type path_fun_key(boost::function<void(std::string)> fun, std::string def);
key_type path_fun_key(boost::function<void(std::string)> fun);
key_type bool_fun_key(boost::function<void(bool)> fun, bool def);
key_type bool_fun_key(boost::function<void(bool)> fun);
key_type int_fun_key(boost::function<void(int)> fun, int def);
key_type int_fun_key(boost::function<void(int)> fun);

key_type fun_values_path(boost::function<void(std::string, std::string)> fun);
key_type string_map_path(std::map<std::string, std::string>* val);

enum type_of_key { key_type_string, key_type_int, key_type_bool, key_type_file, key_type_password, key_type_path, key_type_template };
struct description_container {
  type_of_key type;
  std::string title;
  std::string description;
  bool advanced;
  std::string icon;
  description_container() : type(key_type_path), advanced(false) {}

  description_container(const type_of_key type, std::string title, std::string description, const bool advanced)
      : type(type), title(std::move(title)), description(std::move(description)), advanced(advanced) {}
  description_container(const type_of_key type, std::string title, std::string description, const bool advanced, std::string icon)
      : type(type), title(std::move(title)), description(std::move(description)), advanced(advanced), icon(std::move(icon)) {}
  description_container(const type_of_key type, std::string title, std::string description)
      : type(type), title(std::move(title)), description(std::move(description)), advanced(false) {}

  description_container(const description_container& obj) = default;
  description_container& operator=(const description_container& obj) = default;
};

class settings_registry;
struct path_info;
class settings_paths_easy_init {
 public:
  explicit settings_paths_easy_init(settings_registry* owner) : owner(owner), is_sample(false) {}
  settings_paths_easy_init(std::string path, settings_registry* owner) : path_(std::move(path)), owner(owner), is_sample(false) {}
  settings_paths_easy_init(std::string path, settings_registry* owner, const bool is_sample) : path_(std::move(path)), owner(owner), is_sample(is_sample) {}

  settings_paths_easy_init& operator()(key_type value, std::string title, std::string description, std::string subkeytitle, std::string subkeydescription);
  settings_paths_easy_init& operator()(std::string title, std::string description);
  settings_paths_easy_init& operator()(std::string path, std::string title, std::string description);
  settings_paths_easy_init& operator()(std::string path, key_type value, std::string title, std::string description);
  settings_paths_easy_init& operator()(std::string path, key_type value, std::string title, std::string description, std::string subkeytitle,
                                       std::string subkeydescription);

 private:
  void add(const boost::shared_ptr<path_info>& d) const;

  std::string path_;
  settings_registry* owner;
  bool is_sample;
};

struct tpl_info;
class settings_tpl_easy_init {
 public:
  settings_tpl_easy_init(std::string path, settings_registry* owner) : path_(std::move(path)), owner(owner), is_sample(false) {}

  settings_tpl_easy_init& operator()(std::string path, std::string icon, std::string title, std::string desc, std::string fields);

 private:
  void add(const boost::shared_ptr<tpl_info>& d) const;

  std::string path_;
  settings_registry* owner;
  bool is_sample;
};

struct key_info;
class settings_keys_easy_init {
 public:
  explicit settings_keys_easy_init(settings_registry* owner_) : owner(owner_), is_sample(false) {}
  settings_keys_easy_init(std::string path, settings_registry* owner_) : owner(owner_), path_(std::move(path)), is_sample(false) {}
  settings_keys_easy_init(std::string path, settings_registry* owner_, const bool is_sample) : owner(owner_), path_(std::move(path)), is_sample(is_sample) {}
  settings_keys_easy_init(std::string path, std::string parent, settings_registry* owner_)
      : owner(owner_), path_(std::move(path)), parent_(std::move(parent)), is_sample(false) {}
  settings_keys_easy_init(std::string path, std::string parent, settings_registry* owner_, const bool is_sample)
      : owner(owner_), path_(std::move(path)), parent_(std::move(parent)), is_sample(is_sample) {}

  virtual ~settings_keys_easy_init() = default;

  settings_keys_easy_init& add_string(std::string key_name, key_type value, std::string title, std::string description, bool advanced = false);
  settings_keys_easy_init& add_bool(std::string key_name, key_type value, std::string title, std::string description, bool advanced = false);
  settings_keys_easy_init& add_int(std::string key_name, key_type value, std::string title, std::string description, bool advanced = false);
  settings_keys_easy_init& add_file(std::string key_name, key_type value, std::string title, std::string description, bool advanced = false);
  settings_keys_easy_init& add_password(std::string key_name, key_type value, std::string title, std::string description, bool advanced = false);

 private:
  void add(const boost::shared_ptr<key_info>& d) const;

  settings_registry* owner;
  std::string path_;
  std::string parent_;
  bool is_sample;
};

class path_extension {
 public:
  path_extension(settings_registry* owner, std::string path) : owner_(owner), path_(std::move(path)), is_sample(false) {}

  settings_keys_easy_init add_key_to_path(const std::string& path) { return {get_path(path), owner_, is_sample}; }
  settings_keys_easy_init add_key() { return {path_, owner_, is_sample}; }
  settings_paths_easy_init add_path(const std::string& path = "") { return {get_path(path), owner_, is_sample}; }
  std::string get_path(const std::string& path) {
    if (!path.empty()) return path_ + "/" + path;
    return path_;
  }
  void set_sample() { is_sample = true; }

 private:
  settings_registry* owner_;
  std::string path_;
  bool is_sample;
};
class alias_extension {
 public:
  alias_extension(settings_registry* owner, std::string alias) : owner_(owner), alias_(std::move(alias)) {}
  alias_extension(const alias_extension& other) = default;
  alias_extension& operator=(const alias_extension& other) = default;

  settings_keys_easy_init add_key_to_path(const std::string& path) { return {get_path(path), parent_, owner_}; }
  settings_paths_easy_init add_path(const std::string& path) { return {get_path(path), owner_}; }
  std::string get_path(const std::string& path = "") const {
    if (path.empty()) return "/" + alias_;
    return path + "/" + alias_;
  }

  settings_keys_easy_init add_key_to_settings(const std::string& path = "") { return {get_settings_path(path), parent_, owner_}; }
  settings_paths_easy_init add_path_to_settings(const std::string& path = "") { return {get_settings_path(path), owner_}; }
  settings_tpl_easy_init add_templates(const std::string& path = "") { return {get_settings_path(path), owner_}; }
  std::string get_settings_path(const std::string& path) const {
    if (path.empty()) return "/settings/" + alias_;
    return "/settings/" + alias_ + "/" + path;
  }

  alias_extension add_parent(const std::string& parent_path) {
    set_parent_path(parent_path);
    return *this;
  }

  static std::string get_alias(std::string cur, std::string def) {
    if (cur.empty()) return def;
    return cur;
  }
  static std::string get_alias(std::string prefix, const std::string& cur, const std::string& def) {
    if (!prefix.empty()) prefix += "/";
    if (cur.empty()) return prefix + def;
    return prefix + cur;
  }
  void set_alias(const std::string& cur, const std::string& def) { alias_ = get_alias(cur, def); }
  void set_alias(const std::string& prefix, const std::string& cur, const std::string& def) { alias_ = get_alias(prefix, cur, def); }
  void set_parent_path(const std::string& parent) { parent_ = parent; }

 private:
  settings_registry* owner_;
  std::string alias_;
  std::string parent_;
};

class settings_registry {
  typedef std::list<boost::shared_ptr<key_info> > key_list;
  typedef std::list<boost::shared_ptr<path_info> > path_list;
  typedef std::list<boost::shared_ptr<tpl_info> > tpl_list_type;
  key_list keys_;
  tpl_list_type tpl_;
  path_list paths_;
  settings_impl_interface_ptr core_;
  std::string alias_;

 public:
  explicit settings_registry(settings_impl_interface_ptr core) : core_(std::move(core)) {}
  virtual ~settings_registry() = default;
  settings_impl_interface_ptr get_settings() { return core_; }
  void add(const boost::shared_ptr<key_info>& info) { keys_.push_back(info); }
  void add(const boost::shared_ptr<tpl_info>& info) { tpl_.push_back(info); }
  void add(const boost::shared_ptr<path_info>& info) { paths_.push_back(info); }

  settings_keys_easy_init add_key() { return {settings_keys_easy_init(this)}; }
  settings_keys_easy_init add_key_to_path(const std::string& path) { return {path, this}; }
  settings_keys_easy_init add_key_to_settings(const std::string& path) { return {"/settings/" + path, this}; }
  settings_paths_easy_init add_path() { return {settings_paths_easy_init(this)}; }
  settings_paths_easy_init add_path_to_settings() { return {"/settings", this}; }

  void set_alias(const std::string& cur, const std::string& def) { alias_ = alias_extension::get_alias(cur, def); }
  void set_alias(const std::string& prefix, const std::string& cur, const std::string& def) { alias_ = alias_extension::get_alias(prefix, cur, def); }
  void set_alias(const std::string& alias) { alias_ = alias; }
  alias_extension alias() { return {this, alias_}; }
  alias_extension alias(const std::string& alias) { return {this, alias}; }
  alias_extension alias(const std::string& cur, const std::string& def) { return {this, alias_extension::get_alias(cur, def)}; }
  alias_extension alias(const std::string& prefix, const std::string& cur, const std::string& def) {
    return {this, alias_extension::get_alias(prefix, cur, def)};
  }

  path_extension path(const std::string& path) { return {this, path}; }

  void set_static_key(const std::string& path, const std::string& key, const std::string& value) const { core_->set_string(path, key, value); }
  std::string get_static_string(const std::string& path, const std::string& key, const std::string& def_value) const {
    return core_->get_string(path, key, def_value);
  }

  void register_key_string(const std::string& path, const std::string& key, const std::string& title, const std::string& description,
                           const std::string& defaultValue) const {
    core_->register_key(path, key, "string", title, description, defaultValue, false, false, false);
  }
  void register_key_bool(const std::string& path, const std::string& key, const std::string& title, const std::string& description,
                         const std::string& defaultValue) const {
    core_->register_key(path, key, "bool", title, description, defaultValue, false, false, false);
  }
  void register_key_int(const std::string& path, const std::string& key, const std::string& title, const std::string& description,
                        const std::string& defaultValue) const {
    core_->register_key(path, key, "int", title, description, defaultValue, false, false, false);
  }
  void register_key_file(const std::string& path, const std::string& key, const std::string& title, const std::string& description,
                         const std::string& defaultValue) const {
    core_->register_key(path, key, "file", title, description, defaultValue, false, false, false);
  }
  void register_key_password(const std::string& path, const std::string& key, const std::string& title, const std::string& description,
                             const std::string& defaultValue) const {
    core_->register_key(path, key, "password", title, description, defaultValue, false, false, true);
  }
  void register_all() const;
  void clear() {
    keys_.clear();
    paths_.clear();
  }

  std::string expand_path(const std::string& path) const { return core_->expand_path(path); }

  void notify() const;
};
}  // namespace settings_helper
}  // namespace nscapi