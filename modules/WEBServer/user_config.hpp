// SPDX-FileCopyrightText: 2004-2026 Michael Medin
// SPDX-License-Identifier: Apache-2.0 OR GPL-2.0-only

#pragma once

#include <nscapi/settings/helper.hpp>
#include <nscapi/settings/object.hpp>
#include <nscapi/settings/proxy.hpp>

namespace web_server {
namespace sh = nscapi::settings_helper;

struct user_config_object : public nscapi::settings_objects::object_instance_interface {
  typedef nscapi::settings_objects::object_instance_interface parent;

  std::string password;
  std::string role;

  user_config_object(std::string alias, std::string path) : parent(alias, path) {}

  void read(nscapi::settings_helper::settings_impl_interface_ptr proxy, bool oneliner, bool is_sample) {
    parent::read(proxy, oneliner, is_sample);

    nscapi::settings_helper::settings_registry settings(proxy);

    nscapi::settings_helper::path_extension root_path = settings.path(get_path());
    if (is_sample) root_path.set_sample();

    root_path.add_key()
        .add_password("password", sh::string_key(&password), "PASSWORD", "The password to use.")
        .add_string("role", sh::string_key(&role), "ROLE", "The role which will grant access to this user");

    settings.register_all();
    settings.notify();
  }
};
typedef std::shared_ptr<user_config_object> user_config_instance;

typedef nscapi::settings_objects::object_handler<user_config_object> user_config;

}  // namespace web_server