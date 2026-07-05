// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#include <boost/optional.hpp>
#include <nscapi/macros.hpp>
#include <nscapi/nscapi_helper_singleton.hpp>
#include <nscapi/nscapi_plugin_impl.hpp>
#include <nscapi/nscapi_plugin_wrapper.hpp>
#include <nscapi/protobuf/command.hpp>
#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/object.hpp>

struct wmi_target_object : nscapi::settings_objects::object_instance_interface {
  typedef object_instance_interface parent;

  wmi_target_object(std::string alias, std::string path) : parent(std::move(alias), std::move(path)) {}
  wmi_target_object(const nscapi::settings_objects::object_instance other, std::string alias, std::string path)
      : parent(other, std::move(alias), std::move(path)) {}

  std::string get_hostname() {
    std::string h = get_property_string("hostname");
    if (!h.empty()) return h;
    if (!get_value().empty()) return get_value();
    return get_alias();
  }
  std::string get_username() { return get_property_string("username"); }
  std::string get_password() { return get_property_string("password"); }

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) override {
    parent::read(proxy, oneliner, is_sample);
    if (oneliner) return;

    nscapi::settings_helper::settings_registry settings(proxy);
    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    root_path.add_path()("TARGET", "Target definition for: " + get_alias());

    root_path.add_key()
        .add_string("hostname", nscapi::settings_helper::string_fun_key([this](const std::string &v) { this->set_property_string("hostname", v); }),
                    "TARGET HOSTNAME", "Hostname or ip address of target")
        .add_string("username", nscapi::settings_helper::string_fun_key([this](const std::string &v) { this->set_property_string("username", v); }),
                    "TARGET USERNAME", "Username used to authenticate with")
        .add_password("password", nscapi::settings_helper::string_fun_key([this](const std::string &v) { this->set_property_string("password", v); }),
                      "TARGET PASSWORD", "Password used to authenticate with");

    settings.register_all();
    settings.notify();
  }
};

struct target_helper {
  struct target_info {
    std::string hostname;
    std::string username;
    std::string password;

    std::string to_string() const { return "hostname: " + hostname + ", username: " + username + ", password: " + (password.empty() ? "<unset>" : "<set>"); }
    void update_from(const target_info &other) {
      if (hostname.empty()) hostname = other.hostname;
      if (username.empty()) username = other.username;
      if (password.empty()) password = other.password;
    }
  };

  nscapi::settings_objects::object_handler<wmi_target_object> objects;

  void set_path(const std::string &path) { objects.set_path(path); }

  void add_target(nscapi::settings_helper::settings_impl_interface_ptr proxy, const std::string &key, const std::string &value) {
    try {
      objects.add(proxy, key, value);
    } catch (const std::exception &e) {
      NSC_LOG_ERROR_EXR("Failed to add target: " + key, e);
    } catch (...) {
      NSC_LOG_ERROR_EX("Failed to add target: " + key);
    }
  }

  void finalize(nscapi::settings_helper::settings_impl_interface_ptr proxy) { objects.ensure_default(proxy); }

  boost::optional<target_info> find(const std::string &alias) const {
    if (alias.empty()) return boost::optional<target_info>();
    const auto obj = objects.find_object(alias);
    if (!obj) return boost::optional<target_info>();
    target_info ti;
    ti.hostname = obj->get_hostname();
    ti.username = obj->get_username();
    ti.password = obj->get_password();
    NSC_DEBUG_MSG_STD("Found target: " + ti.to_string());
    return ti;
  }
};

class CheckWMI : public nscapi::impl::simple_plugin {
 public:
  CheckWMI() {}
  virtual ~CheckWMI() {}
  // Module calls
  bool loadModuleEx(std::string alias, NSCAPI::moduleLoadMode mode);
  bool unloadModule();

  // Checks
  void check_wmi(const PB::Commands::QueryRequestMessage::Request &request, PB::Commands::QueryResponseMessage::Response *response);
  NSCAPI::nagiosReturn commandLineExec(int target_mode, const std::string &command, const std::list<std::string> &arguments, std::string &result);

 private:
  target_helper targets;
};
