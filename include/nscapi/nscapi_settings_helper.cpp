/*
 * Copyright 2004-2016 The NSClient++ Authors - https://nsclient.org
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <nscapi/nscapi_settings_helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <settings/settings_core.hpp>
#include <utf8.hpp>
#include <utility>

namespace nscapi {
namespace settings_helper {
namespace s = settings;

inline std::string make_skey(const std::string &path, const std::string &key) { return path + "." + key; }

struct post_processor {
  virtual std::string process(settings_impl_interface_ptr core, std::string value) = 0;
  virtual ~post_processor() = default;
};

struct store_functor {
  virtual void store(std::string value) = 0;
  virtual ~store_functor() = default;
};
struct store_bin_functor {
  virtual void store(std::string key, std::string value) = 0;
  virtual ~store_bin_functor() = default;
};

typedef boost::shared_ptr<store_functor> store_ptr_t;
typedef boost::shared_ptr<post_processor> post_ptr;
typedef boost::shared_ptr<store_bin_functor> bin_ptr;

//////////////////////////////////////////////////////////////////////////
//
// Basic type implementations
//

class typed_key : public key_interface {
  typedef store_ptr_t store_ptr;
  typedef boost::shared_ptr<post_processor> post_ptr;

  bool has_default_;
  std::string default_value_;

  store_ptr store_functor_;
  post_ptr post_functor_;

 public:
  explicit typed_key(store_functor *store_functor) : has_default_(false), store_functor_(store_ptr(store_functor)) {}
  typed_key(store_functor *store_functor, std::string default_value)
      : has_default_(true), default_value_(std::move(default_value)), store_functor_(store_ptr(store_functor)) {}
  typed_key(store_functor *store_functor, post_processor *post_functor)
      : has_default_(false), store_functor_(store_ptr(store_functor)), post_functor_(post_ptr(post_functor)) {}
  typed_key(store_functor *store_functor, std::string default_value, post_processor *post_functor)
      : has_default_(true), default_value_(std::move(default_value)), store_functor_(store_ptr(store_functor)), post_functor_(post_ptr(post_functor)) {}

  std::string get_default() const override { return default_value_; }
  void update_target(const std::string &value) const {
    if (store_functor_) store_functor_->store(value);
  }
  void notify_path(settings_impl_interface_ptr core_, std::string path) const override { throw nsclient::nsclient_exception("Not implemented: notify_path"); }

  void notify(const settings_impl_interface_ptr core_, std::string path, std::string key) const override {
    std::string dummy("$$DUMMY_VALUE_DO_NOT_USE$$");
    if (has_default_) dummy = default_value_;
    std::string data = core_->get_string(path, key, dummy);
    if (has_default_ || data != dummy) {
      try {
        if (post_functor_) {
          data = post_functor_->process(core_, data);
        }
        this->update_target(data);
      } catch (const std::exception &e) {
        core_->err(__FILE__, __LINE__, "Failed to parse key: " + make_skey(path, key) + ": " + utf8::utf8_from_native(e.what()));
      }
    }
  }
  void notify(const settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const override {
    std::string dummy("$$DUMMY_VALUE_DO_NOT_USE$$");
    if (has_default_) dummy = default_value_;
    std::string data = core_->get_string(parent, key, dummy);
    if (has_default_ || data != dummy) dummy = data;
    data = core_->get_string(path, key, dummy);
    if (has_default_ || data != "$$DUMMY_VALUE_DO_NOT_USE$$") {
      try {
        if (post_functor_) {
          data = post_functor_->process(core_, data);
        }
        this->update_target(data);
      } catch (const std::exception &e) {
        core_->err(__FILE__, __LINE__, "Failed to parse key: " + make_skey(path, key) + ": " + utf8::utf8_from_native(e.what()));
      }
    }
  }
};

struct lookup_path_processor : post_processor {
  std::string process(const settings_impl_interface_ptr core_, const std::string value) override { return core_->expand_path(value); }
};

class typed_kvp_value : public key_interface {
  bin_ptr store_functor_;

 public:
  explicit typed_kvp_value(store_bin_functor *store_functor) : store_functor_(bin_ptr(store_functor)) {}

  std::string get_default() const override { return ""; }

  void notify(settings_impl_interface_ptr core_, std::string path, std::string key) const override {
    throw nsclient::nsclient_exception("Not implemented: notify");
  }
  void notify(settings_impl_interface_ptr core_, std::string parent, std::string path, std::string key) const override {
    throw nsclient::nsclient_exception("Not implemented: notify");
  }

  void notify_path(const settings_impl_interface_ptr core_, std::string path) const override {
    if (store_functor_) {
      for (const std::string &key : core_->get_keys(path)) {
        const std::string val = core_->get_string(path, key, "");
        store_functor_->store(key, val);
      }
      for (const std::string &key : core_->get_sections(path)) {
        store_functor_->store(key, "");
      }
    }
  }
};

struct string_storer : store_functor {
  std::string *store_to_;
  explicit string_storer(std::string *store_to) : store_to_(store_to) {}
  void store(const std::string value) override {
    if (store_to_) *store_to_ = value;
  }
};
template <class T>
struct int_storer : store_functor {
  T *store_to_;
  explicit int_storer(T *store_to) : store_to_(store_to) {}
  void store(std::string value) override {
    if (store_to_) *store_to_ = str::stox<T>(value, 0);
  }
};
struct bool_storer : store_functor {
  bool *store_to_;
  explicit bool_storer(bool *store_to) : store_to_(store_to) {}
  void store(const std::string value) override {
    if (store_to_) *store_to_ = s::settings_value::to_bool(value);
  }
};

struct path_storer : store_functor {
  boost::filesystem::path *store_to_;
  explicit path_storer(boost::filesystem::path *store_to) : store_to_(store_to) {}
  void store(const std::string value) override {
    if (store_to_) *store_to_ = value;
  }
};

struct string_fun_storer : store_functor {
  typedef boost::function<void(std::string)> fun_type;
  fun_type callback_;
  explicit string_fun_storer(fun_type callback) : callback_(std::move(callback)) {}
  void store(const std::string value) override {
    if (callback_) callback_(value);
  }
};
struct cstring_fun_storer : store_functor {
  typedef boost::function<void(const char *)> fun_type;
  fun_type callback_;
  explicit cstring_fun_storer(fun_type callback) : callback_(std::move(callback)) {}
  void store(const std::string value) override {
    if (callback_) callback_(value.c_str());
  }
};
template <class T>
struct int_fun_storer : store_functor {
  typedef boost::function<void(T)> fun_type;
  fun_type callback_;
  explicit int_fun_storer(fun_type callback) : callback_(std::move(callback)) {}
  void store(std::string value) override {
    if (callback_) callback_(str::stox<T>(value, -1));
  }
};
struct bool_fun_storer : store_functor {
  typedef boost::function<void(bool)> fun_type;
  fun_type callback_;
  explicit bool_fun_storer(fun_type callback) : callback_(std::move(callback)) {}
  void store(const std::string value) override {
    if (callback_) callback_(s::settings_value::to_bool(value));
  }
};

key_type string_fun_key(boost::function<void(std::string)> fun, std::string def) {
  key_type r(new typed_key(new string_fun_storer(std::move(fun)), std::move(def)));
  return r;
}
key_type string_fun_key(boost::function<void(std::string)> fun) {
  key_type r(new typed_key(new string_fun_storer(std::move(fun))));
  return r;
}
key_type cstring_fun_key(boost::function<void(const char *)> fun, std::string def) {
  key_type r(new typed_key(new cstring_fun_storer(std::move(fun)), std::move(def)));
  return r;
}
key_type cstring_fun_key(boost::function<void(const char *)> fun) {
  key_type r(new typed_key(new cstring_fun_storer(std::move(fun))));
  return r;
}

key_type path_fun_key(boost::function<void(std::string)> fun, std::string def) {
  key_type r(new typed_key(new string_fun_storer(std::move(fun)), std::move(def), new lookup_path_processor()));
  return r;
}
key_type path_fun_key(boost::function<void(std::string)> fun) {
  key_type r(new typed_key(new string_fun_storer(std::move(fun)), new lookup_path_processor()));
  return r;
}

key_type bool_fun_key(boost::function<void(bool)> fun, const bool def) {
  key_type r(new typed_key(new bool_fun_storer(std::move(fun)), s::settings_value::from_bool(def)));
  return r;
}
key_type bool_fun_key(boost::function<void(bool)> fun) {
  key_type r(new typed_key(new bool_fun_storer(std::move(fun))));
  return r;
}

key_type int_fun_key(boost::function<void(int)> fun, const int def) {
  key_type r(new typed_key(new int_fun_storer<int>(std::move(fun)), s::settings_value::from_int(def)));
  return r;
}
key_type int_fun_key(boost::function<void(int)> fun) {
  key_type r(new typed_key(new int_fun_storer<int>(std::move(fun))));
  return r;
}

key_type path_key(std::string *val, std::string def) {
  key_type r(new typed_key(new string_storer(val), std::move(def), new lookup_path_processor()));
  return r;
}
key_type path_key(std::string *val) {
  key_type r(new typed_key(new string_storer(val), new lookup_path_processor()));
  return r;
}
key_type path_key(boost::filesystem::path *val, std::string def) {
  key_type r(new typed_key(new path_storer(val), std::move(def), new lookup_path_processor()));
  return r;
}
key_type path_key(boost::filesystem::path *val) {
  key_type r(new typed_key(new path_storer(val), new lookup_path_processor()));
  return r;
}
key_type string_key(std::string *val, const std::string &def) {
  key_type r(new typed_key(new string_storer(val), def));
  return r;
}
key_type string_key(std::string *val) {
  key_type r(new typed_key(new string_storer(val)));
  return r;
}
key_type int_key(int *val, const int def) {
  key_type r(new typed_key(new int_storer<int>(val), s::settings_value::from_int(def)));
  return r;
}
key_type size_key(std::size_t *val, const std::size_t def) {
  key_type r(new typed_key(new int_storer<std::size_t>(val), str::xtos<std::size_t>(def)));
  return r;
}
key_type int_key(int *val) {
  key_type r(new typed_key(new int_storer<int>(val)));
  return r;
}
key_type uint_key(unsigned int *val, const unsigned int def) {
  key_type r(new typed_key(new int_storer<unsigned int>(val), s::settings_value::from_int(def)));
  return r;
}
key_type uint_key(unsigned int *val) {
  key_type r(new typed_key(new int_storer<unsigned int>(val)));
  return r;
}
key_type bool_key(bool *val, const bool def) {
  key_type r(new typed_key(new bool_storer(val), s::settings_value::from_bool(def)));
  return r;
}
key_type bool_key(bool *val) {
  key_type r(new typed_key(new bool_storer(val)));
  return r;
}

struct map_storer : store_bin_functor {
  typedef std::map<std::string, std::string> map_type;
  map_type *store_to_;
  explicit map_storer(map_type *store_to) : store_to_(store_to) {}
  void store(const std::string key, const std::string value) override {
    if (store_to_ && !value.empty()) (*store_to_)[key] = value;
  }
};
struct kvp_storer : store_bin_functor {
  typedef boost::function<void(std::string, std::string)> fun_type;
  fun_type callback_;
  explicit kvp_storer(fun_type callback) : callback_(std::move(callback)) {}
  void store(const std::string key, const std::string value) override {
    if (callback_) callback_(key, value);
  }
};

key_type fun_values_path(boost::function<void(std::string, std::string)> fun) {
  key_type r(new typed_kvp_value(new kvp_storer(std::move(fun))));
  return r;
}
key_type string_map_path(std::map<std::string, std::string> *val) {
  key_type r(new typed_kvp_value(new map_storer(val)));
  return r;
}
//////////////////////////////////////////////////////////////////////////
//
// Helper classes
//

struct path_info {
  virtual ~path_info() = default;
  std::string path_name;
  key_type path;
  description_container description;
  description_container subkey_description;
  bool is_subkey;
  bool is_sample;

  path_info(std::string path_name, const description_container &description)
      : path_name(std::move(path_name)), description(description), is_subkey(false), is_sample(false) {}
  path_info(std::string path_name, key_type path, const description_container &description)
      : path_name(std::move(path_name)), path(std::move(path)), description(description), is_subkey(false), is_sample(false) {}
  path_info(std::string path_name, key_type path, const description_container &description, const description_container &subkey_description)
      : path_name(std::move(path_name)),
        path(std::move(path)),
        description(description),
        subkey_description(subkey_description),
        is_subkey(true),
        is_sample(false) {}

  path_info(const path_info &obj) = default;
  path_info &operator=(const path_info &obj) = default;
};
struct tpl_info {
  virtual ~tpl_info() = default;
  std::string path_name;
  description_container description;
  std::string fields;

  tpl_info(std::string path_name, const description_container &description, std::string fields)
      : path_name(std::move(path_name)), description(description), fields(std::move(fields)) {}

  tpl_info(const tpl_info &obj) = default;
  tpl_info &operator=(const tpl_info &obj) = default;
};

struct key_info {
  virtual ~key_info() = default;
  std::string path;
  std::string key_name;
  key_type key;
  description_container description;
  std::string parent;
  bool is_sample;
  bool sensitive;

  key_info(std::string path_, std::string key_name_, key_type key, const description_container &description_)
      : path(std::move(path_)), key_name(std::move(key_name_)), key(std::move(key)), description(description_), is_sample(false), sensitive(false) {}
  key_info(const key_info &obj) = default;
  key_info &operator=(const key_info &obj) = default;
  void set_parent(std::string parent_) { parent = std::move(parent_); }
  bool has_parent() const { return !parent.empty(); }
  std::string get_parent() const { return parent; }
};

settings_paths_easy_init &settings_paths_easy_init::operator()(key_type value, std::string title, std::string description, std::string subkeytitle,
                                                               std::string subkeydescription) {
  const boost::shared_ptr<path_info> d(new path_info(path_, std::move(value), description_container(key_type_path, std::move(title), std::move(description)),
                                                     description_container(key_type_path, std::move(subkeytitle), std::move(subkeydescription))));
  add(d);
  return *this;
}

settings_paths_easy_init &settings_paths_easy_init::operator()(std::string title, std::string description) {
  const boost::shared_ptr<path_info> d(new path_info(path_, description_container(key_type_path, std::move(title), std::move(description))));
  add(d);
  return *this;
}

settings_paths_easy_init &settings_paths_easy_init::operator()(std::string path, std::string title, std::string description) {
  if (!path_.empty()) path = path_ + "/" + path;
  const boost::shared_ptr<path_info> d(new path_info(path, description_container(key_type_path, std::move(title), std::move(description))));
  add(d);
  return *this;
}

settings_paths_easy_init &settings_paths_easy_init::operator()(std::string path, key_type value, std::string title, std::string description,
                                                               std::string subkeytitle, std::string subkeydescription) {
  if (!path_.empty()) path = path_ + "/" + path;
  const boost::shared_ptr<path_info> d(new path_info(path, std::move(value), description_container(key_type_path, std::move(title), std::move(description)),
                                                     description_container(key_type_path, std::move(subkeytitle), std::move(subkeydescription))));
  add(d);
  return *this;
}

settings_paths_easy_init &settings_paths_easy_init::operator()(std::string path, key_type value, std::string title, std::string description) {
  if (!path_.empty()) path = path_ + "/" + path;
  const boost::shared_ptr<path_info> d(new path_info(path, std::move(value), description_container(key_type_path, std::move(title), std::move(description))));
  add(d);
  return *this;
}

void settings_paths_easy_init::add(const boost::shared_ptr<path_info> &d) const {
  if (is_sample) d->is_sample = true;
  owner->add(d);
}

settings_tpl_easy_init &settings_tpl_easy_init::operator()(std::string path, std::string icon, std::string title, std::string desc, std::string fields) {
  if (!path_.empty()) path = path_ + "/" + path;
  const boost::shared_ptr<tpl_info> d(
      new tpl_info(path, description_container(key_type_template, std::move(title), std::move(desc), false, std::move(icon)), std::move(fields)));
  add(d);
  return *this;
}

void settings_tpl_easy_init::add(const boost::shared_ptr<tpl_info> &d) const { owner->add(d); }

settings_keys_easy_init &settings_keys_easy_init::add_string(std::string key_name, key_type value, std::string title, std::string description,
                                                             const bool advanced /*= false*/) {
  const boost::shared_ptr<key_info> d(
      new key_info(path_, std::move(key_name), std::move(value), description_container(key_type_string, std::move(title), std::move(description), advanced)));
  if (!parent_.empty()) d->set_parent(parent_);
  add(d);
  return *this;
}

settings_keys_easy_init &settings_keys_easy_init::add_bool(std::string key_name, key_type value, std::string title, std::string description,
                                                           const bool advanced /*= false*/) {
  const boost::shared_ptr<key_info> d(
      new key_info(path_, std::move(key_name), std::move(value), description_container(key_type_bool, std::move(title), std::move(description), advanced)));
  if (!parent_.empty()) d->set_parent(parent_);
  add(d);
  return *this;
}

settings_keys_easy_init &settings_keys_easy_init::add_file(std::string key_name, key_type value, std::string title, std::string description,
                                                           const bool advanced /*= false*/) {
  const boost::shared_ptr<key_info> d(
      new key_info(path_, std::move(key_name), std::move(value), description_container(key_type_file, std::move(title), std::move(description), advanced)));
  if (!parent_.empty()) d->set_parent(parent_);
  add(d);
  return *this;
}

settings_keys_easy_init &settings_keys_easy_init::add_int(std::string key_name, key_type value, std::string title, std::string description,
                                                          const bool advanced /*= false*/) {
  const boost::shared_ptr<key_info> d(
      new key_info(path_, std::move(key_name), std::move(value), description_container(key_type_int, std::move(title), std::move(description), advanced)));
  if (!parent_.empty()) d->set_parent(parent_);
  add(d);
  return *this;
}

settings_keys_easy_init &settings_keys_easy_init::add_password(std::string key_name, key_type value, std::string title, std::string description,
                                                               const bool advanced /*= false*/) {
  const boost::shared_ptr<key_info> d(
      new key_info(path_, std::move(key_name), std::move(value), description_container(key_type_password, std::move(title), std::move(description), advanced)));
  d->sensitive = true;
  if (!parent_.empty()) d->set_parent(parent_);
  add(d);
  return *this;
}

void settings_keys_easy_init::add(const boost::shared_ptr<key_info> &d) const {
  if (is_sample) d->is_sample = true;
  owner->add(d);
}

void settings_registry::register_all() const {
  for (const key_list::value_type &v : keys_) {
    if (v->key) {
      std::list<std::string> paths = {v->path};
      if (v->has_parent()) {
        paths.insert(paths.begin(), v->parent);
      }
      for (const auto &path : paths) {
        if (v->description.type == key_type_bool) {
          core_->register_key(path, v->key_name, "bool", v->description.title, v->description.description, v->key->get_default(), v->description.advanced,
                              v->is_sample, v->sensitive);
        } else if (v->description.type == key_type_int) {
          core_->register_key(path, v->key_name, "int", v->description.title, v->description.description, v->key->get_default(), v->description.advanced,
                              v->is_sample, v->sensitive);
        } else if (v->description.type == key_type_string) {
          core_->register_key(path, v->key_name, "string", v->description.title, v->description.description, v->key->get_default(), v->description.advanced,
                              v->is_sample, v->sensitive);
        } else if (v->description.type == key_type_file) {
          core_->register_key(path, v->key_name, "file", v->description.title, v->description.description, v->key->get_default(), v->description.advanced,
                              v->is_sample, v->sensitive);
        } else if (v->description.type == key_type_password) {
          core_->register_key(path, v->key_name, "password", v->description.title, v->description.description, v->key->get_default(), v->description.advanced,
                              v->is_sample, v->sensitive);
        } else {
          core_->err(__FILE__, __LINE__, "Unknown type for key: " + make_skey(v->path, v->key_name));
        }
      }
    }
  }
  for (const path_list::value_type &v : paths_) {
    core_->register_path(v->path_name, v->description.title, v->description.description, v->description.advanced, v->is_sample);
    if (v->is_subkey) {
      core_->register_subkey(v->path_name, v->subkey_description.title, v->subkey_description.description, v->subkey_description.advanced, true);
    }
  }
  for (const tpl_list_type::value_type &v : tpl_) {
    core_->register_tpl(v->path_name, v->description.title, v->description.icon, v->description.description, v->fields);
  }
}

void settings_registry::notify() const {
  for (const key_list::value_type &v : keys_) {
    try {
      if (v->key) {
        if (v->has_parent())
          v->key->notify(core_, v->parent, v->path, v->key_name);
        else
          v->key->notify(core_, v->path, v->key_name);
      }
    } catch (const std::exception &e) {
      core_->err(__FILE__, __LINE__, "Failed to notify " + v->key_name + ": " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      core_->err(__FILE__, __LINE__, "Failed to notify " + v->key_name);
    }
  }
  for (const path_list::value_type &v : paths_) {
    try {
      if (v->path) v->path->notify_path(core_, v->path_name);
    } catch (const std::exception &e) {
      core_->err(__FILE__, __LINE__, "Failed to notify " + v->path_name + ": " + utf8::utf8_from_native(e.what()));
    } catch (...) {
      core_->err(__FILE__, __LINE__, "Failed to notify " + v->path_name);
    }
  }
}

}  // namespace settings_helper
}  // namespace nscapi