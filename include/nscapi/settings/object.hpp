// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once
#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <nscapi/settings/helper.hpp>
#include <nsclient/nsclient_exception.hpp>
#include <settings/client/settings_client_interface.hpp>
#include <str/xtos.hpp>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace nscapi {
namespace settings_objects {
inline void import_string(std::string &object, const std::string &parent) {
  if (object.empty() && !parent.empty()) object = parent;
}

inline std::string make_obj_path(const std::string &base_path, const std::string &alias) { return base_path + "/" + alias; }

// The raw setting strings an object's read() ended up with, keyed by the path
// relative to the object's own section ("" for the section itself) and the key.
typedef std::map<std::pair<std::string, std::string>, std::string> effective_settings_map;

// Reads an object's settings so that a key missing from its own section takes
// the value its parent ended up with, and only a root object (no parent) falls
// back to the built-in default a key was registered with.
//
// The object keeps the raw strings it read (`effective`, copied from the parent
// when it was cloned). A lookup under the object's own section answers with the
// object's own value when the section has the key, else with the inherited raw
// string, else with the caller's default; whatever it answers with is recorded
// for the object's own children. Handing back the raw string, rather than
// leaving the cloned property alone, means a key's post-processing (path
// expansion, `insecure`'s side effects, `host`/`port` folding into `address`)
// runs again on the child exactly as it did on the parent.
//
// Everything else is passed through. `effective` must outlive the proxy: it is
// only meant to live for the duration of one read().
class inheriting_settings_proxy : public settings_helper::settings_impl_interface {
  settings_helper::settings_impl_interface_ptr inner_;
  std::string own_path_;
  effective_settings_map *effective_;

  bool relative_path(const std::string &path, std::string &rel) const {
    if (path == own_path_) {
      rel.clear();
      return true;
    }
    if (path.size() > own_path_.size() && path.compare(0, own_path_.size(), own_path_) == 0 && path[own_path_.size()] == '/') {
      rel = path.substr(own_path_.size());
      return true;
    }
    return false;
  }

 public:
  inheriting_settings_proxy(settings_helper::settings_impl_interface_ptr inner, std::string own_path, effective_settings_map *effective)
      : inner_(std::move(inner)), own_path_(std::move(own_path)), effective_(effective) {}

  settings_helper::settings_impl_interface_ptr inner() const { return inner_; }

  // A proxy that is not an inheriting one is returned as is.
  static settings_helper::settings_impl_interface_ptr unwrap(const settings_helper::settings_impl_interface_ptr &proxy) {
    if (const auto p = std::dynamic_pointer_cast<inheriting_settings_proxy>(proxy)) return p->inner();
    return proxy;
  }

  std::string get_string(const std::string path, const std::string key, const std::string def) override {
    std::string rel;
    if (!effective_ || !relative_path(path, rel)) return inner_->get_string(path, key, def);
    // Presence is told by a value no setting can hold, so an empty string the
    // operator wrote ("ca =") still counts as set and overrides the parent.
    static const std::string missing("\x01$$nscp-inherit-missing$$\x01");
    const std::string own = inner_->get_string(path, key, missing);
    const auto k = std::make_pair(rel, key);
    if (own != missing) {
      (*effective_)[k] = own;
      return own;
    }
    const auto it = effective_->find(k);
    if (it != effective_->end()) return it->second;
    return def;
  }

  void register_path(std::string path, std::string title, std::string description, bool advanced, bool sample) override {
    inner_->register_path(path, title, description, advanced, sample);
  }
  void register_key(std::string path, std::string key, std::string type, std::string title, std::string description, std::string defValue, bool advanced,
                    bool sample, bool sensitive) override {
    inner_->register_key(path, key, type, title, description, defValue, advanced, sample, sensitive);
  }
  void register_subkey(std::string path, std::string title, std::string description, bool advanced, bool sample) override {
    inner_->register_subkey(path, title, description, advanced, sample);
  }
  void register_user_keys(std::string path) override { inner_->register_user_keys(path); }
  void register_tpl(std::string path, std::string title, std::string icon, std::string description, std::string fields) override {
    inner_->register_tpl(path, title, icon, description, fields);
  }
  void set_string(std::string path, std::string key, std::string value) override { inner_->set_string(path, key, value); }
  string_list get_sections(std::string path) override { return inner_->get_sections(path); }
  string_list get_keys(std::string path) override { return inner_->get_keys(path); }
  std::string expand_path(std::string key) override { return inner_->expand_path(key); }
  void remove_key(std::string path, std::string key) override { inner_->remove_key(path, key); }
  void remove_path(std::string path) override { inner_->remove_path(path); }
  void err(const char *file, int line, std::string message) override { inner_->err(file, line, message); }
  void warn(const char *file, int line, std::string message) override { inner_->warn(file, line, message); }
  void info(const char *file, int line, std::string message) override { inner_->info(file, line, message); }
  void debug(const char *file, int line, std::string message) override { inner_->debug(file, line, message); }
};

struct object_instance_interface {
 private:
  std::string alias;
  std::string base_path;
  std::string path;
  bool is_template_;
  std::string parent;
  std::string value;
  std::unordered_map<std::string, std::string> options;
  effective_settings_map effective_settings_;

 public:
  virtual ~object_instance_interface() = default;
  object_instance_interface(const std::string &alias, const std::string &base_path)
      : alias(alias), base_path(base_path), path(make_obj_path(base_path, alias)), is_template_(false), parent("default") {}
  object_instance_interface(const std::shared_ptr<object_instance_interface> &other, const std::string &alias, const std::string &base_path)
      : alias(alias), base_path(base_path), path(make_obj_path(base_path, alias)), is_template_(false), parent(other->alias) {
    value = other->value;
    for (const auto &e : other->options) {
      options.insert(e);
    }
    effective_settings_ = other->effective_settings_;
  }
  object_instance_interface(const object_instance_interface &other) = default;

  bool is_template() const { return is_template_; }
  void make_template(const bool tpl) { is_template_ = tpl; }
  void setup(const std::string &inAlias, const std::string &inPath, const std::string &inParent = "") {
    alias = inAlias;
    path = inPath + "/" + inAlias;
    base_path = inPath;
    if (!inParent.empty()) {
      parent = inParent;
    }
  }
  const std::unordered_map<std::string, std::string> &get_options() const { return options; }

  // True for an object type whose keys a child takes from its parent when its
  // own section leaves them out (see inheriting_settings_proxy). Off by
  // default, so only the types that opt in (client targets) read that way.
  virtual bool inherits_settings() const { return false; }
  effective_settings_map *get_effective_settings() { return &effective_settings_; }

  virtual void read(settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool _is_sample) {
    // `parent`, `is template` and `alias` describe this object itself and are
    // never inherited: a template's children are not templates.
    settings_helper::settings_registry settings(inheriting_settings_proxy::unwrap(proxy));
    if (oneliner) {
      parent = "default";
      make_template(false);
      settings.path(base_path).add_key().add_string(alias, settings_helper::string_key(&value), alias,
                                                    std::string("To configure this create a section under: ") + path, false);
    } else {
      settings.path(path)
          .add_key()
          .add_string("parent", settings_helper::string_key(&parent, "default"), "PARENT", "The parent the target inherits from", true)
          .add_bool("is template", settings_helper::bool_key(&is_template_, false), "IS TEMPLATE",
                    "Declare this object as a template (this means it will not be available as a separate object)", true)
          .add_string("alias", settings_helper::string_key(&alias), "ALIAS", "The alias (service name) to report to server", true);
    }
    settings.register_all();
    settings.notify();
  }
  std::string get_path() const { return path; }
  std::string get_alias() const { return alias; }
  void set_alias(const std::string &s) { alias = s; }

  std::string get_base_path() const { return base_path; }

  virtual std::string to_string() const {
    std::stringstream ss;
    ss << "{alias: " << alias << ", path: " << path << ", is_tpl: " << (is_template_ ? "true" : "false") << ", parent: " << parent << ", value: " << value
       << ", options : { ";
    std::vector<std::pair<std::string, std::string>> sorted_options(options.begin(), options.end());
    std::sort(sorted_options.begin(), sorted_options.end());
    for (const auto &e : sorted_options) {
      ss << e.first << "=" << e.second << ", ";
    }
    ss << "} }";
    return ss.str();
  }
  bool is_default() const { return alias == "default"; }

  virtual void translate(const std::string &key, const std::string &new_value) { options[key] = new_value; }

  virtual void import(std::shared_ptr<object_instance_interface> new_parent) {}

  bool has_option(const std::string &key) const { return options.find(key) != options.end(); }
  void set_property_int(const std::string &key, const int new_value) { translate(key, str::xtos(new_value)); }
  void set_property_bool(const std::string &key, const bool new_value) { translate(key, new_value ? "true" : "false"); }
  void set_property_string(const std::string &key, const std::string &new_value) { translate(key, new_value); }
  int get_property_int(const std::string &key, const int new_value) {
    const auto cit = options.find(key);
    if (cit == options.end()) return new_value;
    return str::stox<int>(cit->second);
  }
  bool get_property_bool(const std::string &key, const bool new_value) {
    const auto cit = options.find(key);
    if (cit == options.end()) return new_value;
    return cit->second == "true";
  }
  std::string get_property_string(const std::string &key, std::string new_value = "") {
    const auto cit = options.find(key);
    if (cit == options.end()) return new_value;
    return cit->second;
  }

  std::string get_value() const { return value; }
  void set_value(const std::string &new_value) { value = new_value; }
};

typedef std::shared_ptr<object_instance_interface> object_instance;

template <class T>
struct object_factory_interface {
  virtual ~object_factory_interface() = default;
  typedef std::shared_ptr<T> object_instance;
  virtual object_instance create(std::string alias, std::string path) = 0;
  virtual object_instance clone(object_instance parent, std::string alias, std::string path) = 0;
};

template <class T>
struct simple_object_factory : object_factory_interface<T> {
  typedef std::shared_ptr<T> object_instance;
  object_instance create(std::string alias, std::string path) override { return std::make_shared<T>(alias, path); }
  object_instance clone(object_instance parent, const std::string alias, const std::string path) override {
    object_instance inst = std::make_shared<T>(*parent);
    if (inst) {
      inst->setup(alias, path, parent->get_alias());
    }
    return inst;
  }
};

template <class T, class TFactory = simple_object_factory<T>>
struct object_handler : boost::noncopyable {
  typedef std::shared_ptr<T> object_instance;
  typedef std::unordered_map<std::string, object_instance> object_map;
  typedef std::list<object_instance> object_list_type;

  object_map objects;
  object_map templates;
  std::shared_ptr<TFactory> factory;
  std::string path;

  object_handler() : factory(std::make_shared<TFactory>()) {}
  explicit object_handler(std::shared_ptr<TFactory> factory) : factory(factory) {}

  void set_path(const std::string &path_) { path = path_; }

  void add_missing(const settings_helper::settings_impl_interface_ptr &proxy, const std::string &alias, const std::string &value) {
    if (has_object(alias)) return;
    add(proxy, alias, value);
  }

  void add_samples(settings_helper::settings_impl_interface_ptr proxy) {
    object_instance tmp = factory->create("sample", path);
    tmp->read(proxy, false, true);
  }

  std::list<object_instance> get_object_list() const {
    std::list<object_instance> ret;
    for (const typename object_map::value_type &t : objects) {
      ret.push_back(t.second);
    }
    return ret;
  }
  std::list<std::string> get_alias_list() const {
    std::list<std::string> ret;
    for (const typename object_map::value_type &t : objects) {
      ret.push_back(t.first);
    }
    return ret;
  }
  bool has_objects() const { return !objects.empty(); }

  void ensure_default(settings_helper::settings_impl_interface_ptr proxy = settings_helper::settings_impl_interface_ptr()) {
    if (has_object("default")) return;
    add(proxy, "default", "");
  }

  object_instance add(settings_helper::settings_impl_interface_ptr proxy, std::string alias, std::string value, const bool force_template = false) {
    bool is_template = (alias == "default") || force_template;
    object_instance previous = find_object(alias);
    if (previous) {
      return previous;
    }

    object_instance object;
    if (proxy) {
      const std::list<std::string> keys = proxy->get_keys(make_obj_path(path, alias));
      const std::string parent_name = proxy->get_string(make_obj_path(path, alias), "parent", "default");
      if (!parent_name.empty() && parent_name != alias) {
        object_instance parent;
        if (has_object(parent_name))
          parent = find_object(parent_name);
        else
          parent = add(proxy, parent_name, "", true);
        if (parent) {
          object = factory->clone(parent, alias, path);
          object->make_template(false);
        } else {
          throw nsclient::nsclient_exception("Failed to create settings object for: " + alias);
        }
      } else {
        object = factory->create(alias, path);
      }
      object->set_value(value);
      settings_helper::settings_impl_interface_ptr read_proxy = proxy;
      if (object->inherits_settings()) {
        // A root object (created, not cloned) starts with nothing to inherit,
        // so it reads its own keys or the built-in defaults as before; going
        // through the proxy still records what it ended up with, for its
        // children.
        read_proxy = std::make_shared<inheriting_settings_proxy>(proxy, object->get_path(), object->get_effective_settings());
      }
      object->read(read_proxy, keys.empty() && alias != "default", false);
    } else {
      object = factory->create(alias, path);
      object->set_value(value);
    }
    if (is_template || object->is_template()) {
      add_template(object);
      if (alias != object->get_alias()) add_template(alias, object);
    } else
      add_object(object);
    return object;
  }

  void materialize() { ensure_default(); }

  bool remove(const settings_helper::settings_impl_interface_ptr &proxy, const std::string &alias) {
    proxy->remove_path(make_obj_path(path, alias));
    proxy->remove_key(path, alias);
    return remove(alias);
  }
  bool remove(const std::string &alias) {
    typename object_map::const_iterator cit = objects.find(alias);
    if (cit != objects.end()) {
      objects.erase(alias);
      return true;
    }
    cit = templates.find(alias);
    if (cit != templates.end()) {
      templates.erase(alias);
      return true;
    }
    return false;
  }

  object_instance find_object(const std::string &alias) const {
    typename object_map::const_iterator cit = objects.find(alias);
    if (cit != objects.end()) return cit->second;
    cit = templates.find(alias);
    if (cit != templates.end()) return cit->second;
    return object_instance();
  }

  bool has_object(std::string alias) const {
    typename object_map::const_iterator cit = objects.find(alias);
    if (cit != objects.end()) {
      return true;
    }
    cit = templates.find(alias);
    if (cit != templates.end()) {
      return true;
    }
    return false;
  }

  bool empty() const { return objects.empty(); }

  void clear() {
    objects.clear();
    templates.clear();
  }

  std::string to_string() const {
    std::stringstream ss;
    ss << "Objects: ";
    for (const typename object_map::value_type &t : objects) {
      ss << ", " << t.first << " = {" << t.second->to_string() + "} ";
    }
    ss << "Templates: ";
    for (const typename object_map::value_type &t : templates) {
      ss << ", " << t.first << " = {" << t.second->to_string() + "} ";
    }
    return ss.str();
  }

  void add_object(object_instance object) { objects[object->get_alias()] = object; }
  void add_object(const std::string &alias, object_instance object) { objects[alias] = object; }
  void add_template(object_instance object) {
    object->make_template(true);
    templates[object->get_alias()] = object;
  }
  void add_template(const std::string &alias, object_instance object) { templates[alias] = object; }
};
}  // namespace settings_objects
}  // namespace nscapi
